/*

  	PsychToolbox3/Source/windows/Base/PsychTimeGlue.c

	AUTHORS:

  	mario.kleiner at tuebingen.mpg.de		mk 

	PLATFORMS: Micro$oft Windows Only


  	PROJECTS:

  	12/27/05	mk		Screen on M$-Windows

  	HISTORY:

  	12/27/05	mk		Wrote it. Derived from OS-X version.  

  	DESCRIPTION:
	
	Functions for querying system time and for waiting for either a
	specified amount of time or until a specified point in time.
	Also returns timer ticks and resolution of timers.

	TO DO:

	Replace the busy-waiting spin-loops in WaitUntilSeconds and WaitIntervalSeconds
	by some proper Win32 call to put process to sleep.

*/



#include "Psych.h"

/*

 *		file local state variables

*/


static double		precisionTimerAdjustmentFactor=1;
static double		estimatedGetSecsValueAtTickCountZero;
static Boolean		isKernelTimebaseFrequencyHzInitialized=FALSE;
static long double	kernelTimebaseFrequencyHz;
static Boolean          counterExists=FALSE;
static Boolean          firstTime=TRUE;
static double           sleepwait_threshold = 0.003;

void PsychWaitUntilSeconds(double whenSecs)
{
  static unsigned int missed_count=0;
  static Boolean firstfail = TRUE;
  double now=0.0;

  // Get current time:
  PsychGetPrecisionTimerSeconds(&now);

  // If the deadline has already passed, we do nothing and return immediately:
  if (now > whenSecs) return;

  // Waiting stage 1: If we have more than sleepwait_threshold seconds left
  // until the deadline, we call the OS Sleep() function, so the
  // CPU gets released for difference - sleepwait_threshold s to other processes and threads.
  // -> Good for general system behaviour and for lowered
  // power-consumption (longer battery runtime for Laptops) as
  // the CPU can go idle if nothing else to do...
  while(whenSecs - now > sleepwait_threshold) {
	 // Try to switch windows timer to 1 msec precision:
    if ((timeBeginPeriod(1)!=TIMERR_NOERROR) && firstfail) {
		  // High precision mode failed! Output warning on first failed invocation...
		  firstfail = FALSE;
        printf("PTB-WARNING: PsychTimeGlue - Win32 syscall timeBeginPeriod(1) failed! Timing may be inaccurate...\n");
		  // Increase switching threshold to 10 msecs to take low timer resolution into account:
		  sleepwait_threshold = 0.010;
    }    

	 // Sleep until only sleepwait_threshold away from deadline:
    Sleep((int)((whenSecs - now - sleepwait_threshold) * 1000.0f));

	 // Switch windows timer back to default precision (around 10-20 msecs):
	 timeEndPeriod(1);

	 // Recheck:
    PsychGetPrecisionTimerSeconds(&now);
  }

  // Waiting stage 2: We are less than sleepwait_threshold s away from deadline.
  // Perform busy-waiting until deadline reached:
  while(now < whenSecs) PsychGetPrecisionTimerSeconds(&now);

  // Check for deadline-miss of more than 1 ms:
  if (now - whenSecs > 0.001) {
    // Deadline missed by over 1 ms.
    missed_count++;

    if (missed_count>5) {
      // Too many consecutive misses. Increase our threshold for sleep-waiting
      // by 5 ms until it reaches 20 ms.
      if (sleepwait_threshold < 0.02) sleepwait_threshold+=0.005;
      printf("PTB-WARNING: Wait-Deadline missed for %i consecutive times (Last miss %lf ms). New sleepwait_threshold is %lf ms.\n",
	     missed_count, (now - whenSecs)*1000.0f, sleepwait_threshold*1000.0f);
		// Reset missed count after increase of threshold:
		missed_count = 0;
    }
  }
  else {
    // No miss detected. Reset counter...
    missed_count=0;
  }

  // Ready.
  return;
}

void PsychWaitIntervalSeconds(double delaySecs)
{
  double deadline;
  // Get current time:
  PsychGetPrecisionTimerSeconds(&deadline);
  // Compute deadline in absolute system time:
  deadline+=delaySecs;
  // Wait until deadline reached:
  PsychWaitUntilSeconds(deadline);
  return;
}

double	PsychGetKernelTimebaseFrequencyHz(void)
{
  if(!isKernelTimebaseFrequencyHzInitialized){
    isKernelTimebaseFrequencyHzInitialized=TRUE;
    PsychGetPrecisionTimerTicksPerSecond(&kernelTimebaseFrequencyHz);
  }
  return((double)kernelTimebaseFrequencyHz);
}

void PsychInitTimeGlue(void)
{
  PsychEstimateGetSecsValueAtTickCountZero();
}

void PsychGetPrecisionTimerTicks(psych_uint64 *ticks)
{
  LARGE_INTEGER	                count;
  if (QueryPerformanceFrequency(&count)) {
    QueryPerformanceCounter(&count);
    *ticks = (psych_uint64) count.QuadPart;
  }
  else {
    *ticks = (psych_uint64) GetTickCount();
  }
  return;
}

void PsychGetPrecisionTimerTicksPerSecond(double *frequency)
{
  LARGE_INTEGER	                counterFreq;

  // High precision timer available?
  if (QueryPerformanceFrequency(&counterFreq)) {
    // Yes. Returns its operating frequency:
    *frequency=(double) counterFreq.QuadPart;
  }
  else {
    // No. Return the 1 khZ tickfreq of the system tick.
    *frequency=1000.0f;
  }
  return;
}

void PsychGetPrecisionTimerTicksMinimumDelta(psych_uint32 *delta)

{
  // FIXME: Don't know if this is correct!
  *delta=1;
}

void PsychGetPrecisionTimerSeconds(double *secs)

{
  // This code is taken from the old Windows Psychtoolbox:
  // (VideoToolbox/SecondsPC.c)
  double				ss;
  static LARGE_INTEGER	                counterFreq;
  LARGE_INTEGER			        count;

  if (firstTime) {
    counterExists = QueryPerformanceFrequency(&counterFreq);
    firstTime = FALSE;
  }
  
  if (counterExists) {
    QueryPerformanceCounter(&count);
    ss = ((double)count.QuadPart)/((double)counterFreq.QuadPart);
  } else {
    ss = (double) GetTickCount();
    ss = ss/1000;
  }
  
  *secs= ss;  
}

void PsychGetAdjustedPrecisionTimerSeconds(double *secs)
{
  double		rawSecs, factor;
  
  PsychGetPrecisionTimerSeconds(&rawSecs);
  PsychGetPrecisionTimerAdjustmentFactor(&factor);
  *secs=rawSecs * precisionTimerAdjustmentFactor;
}

void PsychGetPrecisionTimerAdjustmentFactor(double *factor)
{
  *factor=precisionTimerAdjustmentFactor;
}

void PsychSetPrecisionTimerAdjustmentFactor(double *factor)
{
  precisionTimerAdjustmentFactor=*factor;
}

/*
	PsychEstimateGetSecsValueAtTickCountZero()

	Note that the tick counter rolls over after a couple of months.
	Its theoretically possible to have machine uptime of that long 
	but its extremely unlikely given that this is Microsoft Windows ;)
	so we don't worry about roll over when calculating. 

*/
void PsychEstimateGetSecsValueAtTickCountZero(void)
{
  double		nowTicks, nowSecs;
  
  nowTicks=(double) GetTickCount();
  PsychGetAdjustedPrecisionTimerSeconds(&nowSecs);
  estimatedGetSecsValueAtTickCountZero=nowSecs - nowTicks * (1/1000.0f); 
}

double PsychGetEstimatedSecsValueAtTickCountZero(void)
{
  return(estimatedGetSecsValueAtTickCountZero);
}

