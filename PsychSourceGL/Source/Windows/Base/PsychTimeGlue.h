/*

  	PsychToolbox3/Source/Windows/Base/PsychTimeGlue.h

  	AUTHORS:

  	mario.kleiner@tuebingen.mpg.de	mk

	PLATFORMS: Win32 only

  	PROJECTS:

  	1/20/03	awi		Screen on OS X

  	HISTORY:

  	1/20/03	awi		Wrote it for OS/X.  

  	DESCRIPTION:

	TO DO:

*/

//begin include once 
#ifndef PSYCH_IS_INCLUDED_PsychTimeGlue
#define PSYCH_IS_INCLUDED_PsychTimeGlue

#include "Psych.h"

void PsychWaitUntilSeconds(double whenSecs);
void	PsychWaitIntervalSeconds(double seconds);
double	PsychGetKernelTimebaseFrequencyHz(void);
void PsychGetPrecisionTimerTicks(psych_uint64 *ticks);
void PsychGetPrecisionTimerTicksPerSecond(double *frequency);
void PsychGetPrecisionTimerTicksMinimumDelta(psych_uint32 *delta);
void PsychGetPrecisionTimerSeconds(double *secs);
void PsychGetAdjustedPrecisionTimerSeconds(double *secs);
void PsychGetPrecisionTimerAdjustmentFactor(double *factor);
void PsychSetPrecisionTimerAdjustmentFactor(double *factor);
void PsychInitTimeGlue(void);
void PsychExitTimeGlue(void);
void PsychEstimateGetSecsValueAtTickCountZero(void);
double PsychGetEstimatedSecsValueAtTickCountZero(void);
double PsychGetTimeGetTimeValueAtLastTimeQuery(double* precisionRawtime);
unsigned int PsychGetTimeBaseHealthiness(void);

//end include once
#endif
