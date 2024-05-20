/*
 * Project 64 Legacy - A Nintendo 64 emulator.
 *
 * (c) Copyright 2001 Zilmar, Jabo, Smiff, Gent, Witten 
 * (c) Copyright 2010 PJ64LegacyTeam
 *
 * Project64 Legacy Homepage: www.project64-legacy.com
 *
 * Permission to use, copy, modify and distribute Project64 in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Project64 is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for Project64 or software derived from Project64.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so if they want them.
 *
 */
#include <windows.h>
#include "Language.h"
#include "Win32Timer.h"

/********************* Win32 Thread Timer ********************/

struct {
	DWORD Frames, LastTime;
	DOUBLE Ratio;
} FPSTimer = { 0,0, 1000.0F / 60.0F };

struct RecordedFrame
{
	double calculatedTime;
	DWORD lastTime;
	DWORD currentTime;
	DWORD lastFrames;
	char reset;
};

#define MS_RESET_TIME 70

void Timer_Initialize(double Hertz) {
	FPSTimer.Ratio = 1000.0f / Hertz;
}

void Timer_Start(void) {
	TIMECAPS Caps;
	timeGetDevCaps(&Caps, sizeof(Caps));
	if (timeBeginPeriod(Caps.wPeriodMin) == TIMERR_NOCANDO)
		MessageBox(NULL, "Error during timer begin", GS(MSG_MSGBOX_TITLE), MB_ICONERROR);

	FPSTimer.Frames = 0;
	FPSTimer.LastTime = timeGetTime();
}

void Timer_Stop(void) {
	TIMECAPS Caps;
	timeGetDevCaps(&Caps, sizeof(Caps));
	timeEndPeriod(Caps.wPeriodMin);
}

BOOL Timer_Process(DWORD * FrameRate) {
	if (-2 == FPSTimer.Frames)
	{
		FPSTimer.Frames = -1;
		return FALSE;
	}
	if (-1 == FPSTimer.Frames)
	{
		FPSTimer.Frames = 0;
		FPSTimer.LastTime = Timer_now();
	}

	struct RecordedFrame record = { 0 };

	double CalculatedTime;
	DWORD CurrentTime;

	record.lastFrames = FPSTimer.Frames++;
	record.lastTime = FPSTimer.LastTime;
	record.currentTime = (CurrentTime = Timer_now());

	/* Calculate time that should of elapsed for this frame */
	record.calculatedTime = (CalculatedTime = (double)FPSTimer.LastTime + (FPSTimer.Ratio * (double)FPSTimer.Frames));
	char reset = CurrentTime - FPSTimer.LastTime >= 1000;
	if ((double)CurrentTime < CalculatedTime) {
		long time = (int)(CalculatedTime - (double)CurrentTime);
		if (time > 0) {
			Sleep(time);
		}

		/* Refresh current time */
		CurrentTime = Timer_now();
	}
	else
	{
		// this is a new code - if we are falling very behind, try to reset the timer
		long time = (int)((double)CurrentTime - CalculatedTime);
		reset = time > MS_RESET_TIME;
	}

	record.reset = reset;

#ifdef DEBUG_ENABLE_TIMER_TRACING
	if (sFramesLeftToRecord)
	{
		sRecordedFrames.push_back(record);
		sFramesLeftToRecord--;
	}
	else if (!sRecordedFrames.empty())
	{
		FILE* fd = fopen(getRecordsPath(), "w");
		if (fd)
		{
			for (const auto& record : sRecordedFrames)
			{
				fprintf(fd, "calc=%lf cur=%ld diff=%lf time=%ld frames=%ld%s\n", record.calculatedTime, record.currentTime, record.calculatedTime - record.currentTime, record.lastTime, record.lastFrames, record.reset ? " reset" : "");
			}
			fclose(fd);
		}
		sRecordedFrames.clear();
	}
#endif

	if (reset) {
		/* Output FPS */
		if (FrameRate != NULL) { *FrameRate = FPSTimer.Frames; }
		FPSTimer.Frames = 0;
		FPSTimer.LastTime = CurrentTime;

		return TRUE;
	}
	else
		return FALSE;
}

void Timer_Adjust(DWORD amount)
{
	FPSTimer.LastTime += amount;
}

void Timer_Reset(void)
{
	FPSTimer.Frames = -2;
}
