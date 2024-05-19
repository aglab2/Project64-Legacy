#include "SummerCart.h"

#include <Windows.h>
extern BYTE* N64MEM, * RDRAM, * DMEM, * IMEM, * ROM;

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define S8 3

struct summercart SummerCart;

static uint8_t* summercart_sd_addr(size_t size)
{
    uint32_t addr = SummerCart.data0 & 0x1fffffff;
    if (addr >= 0x1ffe0000 && addr + size < 0x1ffe0000 + 8192)
    {
        return SummerCart.buffer + (addr - 0x1ffe0000);
    }
    if (addr >= 0x10000000 && addr + size < 0x10000000 + 0x4000000)
    {
        return ROM + (addr - 0x10000000);
    }
    return NULL;
}

static uint32_t summercart_sd_init()
{
    FILE* fp;
    if (!SummerCart.sd_path) return 0x40000000;
    if (!(fp = fopen(SummerCart.sd_path, "rb"))) return 0x40000000;
    fseek(fp, 0, SEEK_END);
    SummerCart.sd_size = ftell(fp);
    fclose(fp);
    return 0;
}

static uint32_t summercart_sd_read()
{
    size_t i;
    FILE* fp;
    uint8_t* ptr;
    long offset = 512 * SummerCart.sd_sector;
    size_t size = 512 * SummerCart.data1;
    if (offset + size > SummerCart.sd_size) return 0x40000000;
    if (!(ptr = summercart_sd_addr(size))) return 0x40000000;
    if (!(fp = fopen(SummerCart.sd_path, "rb"))) return 0x40000000;
    fseek(fp, offset, SEEK_SET);
    for (i = 0; i < size; ++i)
    {
        int c = fgetc(fp);
        if (c < 0)
        {
            fclose(fp);
            return 0x40000000;
        }
        ptr[i ^ SummerCart.sd_byteswap ^ S8] = c;
    }
    fclose(fp);
    return 0;
}

static uint32_t summercart_sd_write(struct pi_controller* pi)
{
    size_t i;
    FILE* fp;
    uint8_t* ptr;
    long offset = 512 * SummerCart.sd_sector;
    size_t size = 512 * SummerCart.data1;
    if (offset + size > SummerCart.sd_size) return 0x40000000;
    if (!(ptr = summercart_sd_addr(pi, size))) return 0x40000000;
    if (!(fp = fopen(SummerCart.sd_path, "r+b"))) return 0x40000000;
    fseek(fp, offset, SEEK_SET);
    for (i = 0; i < size; ++i)
    {
        int c = fputc(ptr[i ^ S8], fp);
        if (c < 0)
        {
            fclose(fp);
            return 0x40000000;
        }
    }
    fclose(fp);
    return 0;
}

void init_summercart(struct summercart* summercart)
{
    summercart->sd_path = "D:\\Project64\\AUTO0.iso"; // getenv("PL_SD_CARD_IMAGE");
}

void poweron_summercart(struct summercart* summercart)
{
    memset(summercart->buffer, 0, 8192);
    summercart->sd_size = -1;
    summercart->status = 0;
    summercart->data0 = 0;
    summercart->data1 = 0;
    summercart->sd_sector = 0;
    summercart->cfg_rom_write = 0;
    summercart->sd_byteswap = 0;
    summercart->unlock = 0;
    summercart->lock_seq = 0;
}

int read_summercart_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct pi_controller* pi = (struct pi_controller*)opaque;
    uint32_t addr = address & 0xFFFF;

    *value = 0;

    if (!SummerCart.unlock) return 0;

    switch (address & 0xFFFF)
    {
    case 0x00:  *value = SummerCart.status; break;
    case 0x04:  *value = SummerCart.data0;  break;
    case 0x08:  *value = SummerCart.data1;  break;
    case 0x0C:  *value = 0x53437632;            break;
    }

    return 0;
}

int write_summercart_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct pi_controller* pi = (struct pi_controller*)opaque;
    uint32_t addr = address & 0xFFFF;

    if (addr == 0x10)
    {
        switch (value & mask)
        {
        case 0xFFFFFFFF:
            SummerCart.unlock = 0;
            break;
        case 0x5F554E4C:
            if (SummerCart.lock_seq == 0)
            {
                SummerCart.lock_seq = 2;
            }
            break;
        case 0x4F434B5F:
            if (SummerCart.lock_seq == 2)
            {
                SummerCart.unlock = 1;
                SummerCart.lock_seq = 0;
            }
            break;
        default:
            SummerCart.lock_seq = 0;
            break;
        }
        return 0;
    }

    if (!SummerCart.unlock) return 0;

    switch (addr)
    {
    case 0x00:
        SummerCart.status = 0;
        switch (value & mask)
        {
        case 'c':
            switch (SummerCart.data0)
            {
            case 1:
                SummerCart.data1 = SummerCart.cfg_rom_write;
                break;
            case 3:
                SummerCart.data1 = 0;
                break;
            case 6:
                SummerCart.data1 = 0;
                break;
            default:
                SummerCart.status = 0x40000000;
                break;
            }
            break;
        case 'C':
            switch (SummerCart.data0)
            {
            case 1:
                if (SummerCart.data1)
                {
                    SummerCart.data1 = SummerCart.cfg_rom_write;
                    SummerCart.cfg_rom_write = 1;
                }
                else
                {
                    SummerCart.data1 = SummerCart.cfg_rom_write;
                    SummerCart.cfg_rom_write = 0;
                }
                break;
            default:
                SummerCart.status = 0x40000000;
                break;
            }
            break;
        case 'i':
            switch (SummerCart.data1)
            {
            case 0:
                break;
            case 1:
                SummerCart.status = summercart_sd_init(pi);
                break;
            case 4:
                SummerCart.sd_byteswap = 1;
                break;
            case 5:
                SummerCart.sd_byteswap = 0;
                break;
            default:
                SummerCart.status = 0x40000000;
                break;
            }
            break;
        case 'I':
            SummerCart.sd_sector = SummerCart.data0;
            break;
        case 's':
            SummerCart.status = summercart_sd_read(pi);
            break;
        case 'S':
            SummerCart.status = summercart_sd_write(pi);
            break;
        default:
            SummerCart.status = 0x40000000;
            break;
        }
        break;
    case 0x04:
        SummerCart.data0 = value & mask;
        break;
    case 0x08:
        SummerCart.data1 = value & mask;
        break;
    }

    return 0;
}
