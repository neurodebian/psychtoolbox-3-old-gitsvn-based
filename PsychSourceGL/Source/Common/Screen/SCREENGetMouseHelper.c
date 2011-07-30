/*
	SCREENGetMouseHelper.c	
  
	AUTHORS:

		Allen.Ingling@nyu.edu		awi 
  		mario.kleiner@tuebingen.mpg.de	mk
	PLATFORMS:
	
		All, with platform dependent code and lots of uglyness.
 
	HISTORY:

		10/11/04	awi		Created.  
 		??/??/??	mk		Add support for Windows and Linux, add lots of other stuff... 
 
	DESCRIPTION:
  
		* Returns the position of the mouse pointer.

		* Also does lots of other tasks - this is kind of a trashbin for lazy Mario:
			- Implements KbChecks/KbWaits on Windows and Linux.
			- Implements GetChar on Linux.
			- Implements Priority() for Windows and Linux.
		
		The command syntax and arguments described in the online help do the mouse queries.
		Secret command codes (encoded as negative 'numButtons' arguments) trigger all other
		functions. One needs to read the M-Files for KbCheck, GetChar, Priority etc. to find
		out what means what and why.

	NOTES
	
		We need these ingredients:
		
			1. Read mouse buttons
			
				Options are Button(), GetCurrentEventButtonState() or GetCurrentButtonState().  Or use preferences to set which.
			
			2. Detect number of mouse buttons
			
				Could use PsychHID.
			
			3. Read screen position of mouse.
			
				GetMouse, even though it's deprectated.
			
			4. Read screen position of mouse in CG fullscreen window when resolution changes.
			
				TestGetMouse.  We may have to apply a hack if GetMouse does not pick up on the resolution change. 
				
			5. Get the origin for each onscreen window in global coordinates.
			
					Use:
					CGRect CGDisplayBounds (CGDirectDisplayID display);

					Add:
					oldRect=Screen(windowPtrOrScreenNumber,'GlobalRect',[rect]);

*/


#include "Screen.h"

// Current ListenChar state:
static int	listenchar_enabled = 0;

#if PSYCH_SYSTEM == PSYCH_LINUX

/* These are needed for realtime scheduling and memory locking control: */
#include <sched.h>
#include <errno.h>
#include <sys/mman.h>
#endif

#if PSYCH_SYSTEM != PSYCH_WINDOWS

/**
 POSIX implementation of _kbhit() for Linux and OS/X:
 Adapted from example code by Morgan McGuire, morgan@cs.brown.edu
*/

#include <stdio.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>

int _kbhit(void) {
	struct termios		term;
    static const int	STDIN = 0;
    static int			current_mode = 0;
    int					bytesWaiting;

	// Change of mode requested?
    if (current_mode != listenchar_enabled) {
        // Use termios to turn off line buffering
        tcgetattr(STDIN, &term);

		// MK: Ok, none of these settings works as we'd like it to work :-(
		// The only reasonable case is clearing the ICANON flag, which gives
		// us what we want. All other settings commented out, because they
		// do more harm than good.
		
		if (listenchar_enabled == 0) {
			// Enable canonic input processing - The normal mode:
//			term.c_lflag |= ICANON;
		}
		else {
			// Disable canonic input processing so we don't need to wait
			// for newline before we get input:
			term.c_lflag &= ~ICANON;
		}
		
//		if (listenchar_enabled == 2) {
//			// Disable echoing of characters:
//			term.c_lflag &= ~(ECHO);
//		}
//		else {
//			// Enable echoing of characters:
//			term.c_lflag |= (ECHO);
//		}
		
        tcsetattr(STDIN, TCSANOW, &term);
		
		// Disable bufferin of characters:
        setbuf(stdin, NULL);

		// New opmode established:
        current_mode = listenchar_enabled;
    }

	// Query number of pending characters in stdin stream:
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

#else
// _kbhit() is part of MS-Windows CRT standard runtime library. We just
// need to include the conio header file:
#include <conio.h>
#endif

// Special console handler: Performs functions of ListenChar, FlushEvents,
// CharAvail and GetChar when run in terminal mode without GUI and JavaVM:
void ConsoleInputHelper(int ccode)
{
	int ret;
	
	switch(ccode) {
		case -10:	// ListenChar(0);
			// Disable char listening:
			listenchar_enabled = 0;
			_kbhit();
		break;
		
		case -11:	// ListenChar(1);
			// Enable char listening:
			listenchar_enabled = 1;
			_kbhit();
		break;

		case -12:	// ListenChar(2);
			// Enable char listening and suppress output to console:
			listenchar_enabled = 1 + 2;
			_kbhit();
		break;

		case -13:	// FlushEvents:
			// Enable char listening:
			listenchar_enabled |= 1;

			// Drain queue from all pending characters:
			while (_kbhit()) getchar();
		break;

		case -14:	// CharAvail():
			// Enable char listening:
			listenchar_enabled |= 1;

			// Return number of pending chars in queue:
			PsychCopyOutDoubleArg(1, kPsychArgOptional, (double) _kbhit());
		break;

		case -15:	// GetChar():
			// Enable char listening:
			listenchar_enabled |= 1;
			
			// Retrieve one character: Return if none available.
			// We call _kbhit() once to turn the terminal into non-blocking mode,
			// so it doesn't wait for newlines:
			if (0 == _kbhit()) {
				// Nothing available: Return zero result code:
				ret = 0;
			}
			else {
				// At least one available: Fetch one...
				ret = getchar();
				// Valid?
				if (ret == EOF) {
					// Failed - End of stream or error! Clear error condition:
					clearerr(stdin);
					errno = 0;
					// Return error code -1:
					ret = -1;
				}
			}
			
			// Return whatever we've got:
			PsychCopyOutDoubleArg(1, kPsychArgOptional, (double) ret);
		break;
		
		default:
			PsychErrorExitMsg(PsychError_internal, "Invalid command code encountered in ConsoleInputHelper()! This is an implementation BUG!");
	}

	return;
}

// If you change the useString then also change the corresponding synopsis string in ScreenSynopsis.c
static char useString[] = "[x, y, buttonValueArray]= Screen('GetMouseHelper', numButtons [, screenNumber][, mouseIndex]);";
//                          1  2  3                                           1          2                3
static char synopsisString[] = 
	"This is a helper function called by GetMouse.  Do not call Screen(\'GetMouseHelper\'), use "
	"GetMouse instead.\n"
	"\"numButtons\" is the number of mouse buttons to return in buttonValueArray. 1 <= numButtons <= 32. Ignored on Linux and Windows.\n"
	"\"screenNumber\" is the number of the PTB screen whose mouse should be queried on setups with multiple connected mice. "
	"This value is optional (defaults to zero) and only honored on GNU/Linux. It's meaningless on OS-X or Windows.\n"
	"\"mouseIndex\" is the optional index of the (mouse)pointer device. Defaults to system pointer. Only honored on Linux.\n";
static char seeAlsoString[] = "";
	 

PsychError SCREENGetMouseHelper(void) 
{
#if PSYCH_SYSTEM == PSYCH_OSX
	Point		mouseXY;
	UInt32		buttonState;
	double		*buttonArray;
	int		numButtons, i;
	psych_bool	doButtonArray;
	PsychWindowRecordType *windowRecord;
	
	//all subfunctions should have these two lines.  
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};
	
	//cap the numbers of inputs and outputs
	PsychErrorExit(PsychCapNumInputArgs(2));   //The maximum number of inputs
	PsychErrorExit(PsychCapNumOutputArgs(4));  //The maximum number of outputs
	
	//Buttons.  
	// The only way I know to detect the  number number of mouse buttons is directly via HID.  The device reports
	//that information but OS X seems to ignore it above the level of the HID driver, that is, no OS X API above the HID driver
	//exposes it.  So GetMouse.m function calls PsychHID detect the number of buttons and then passes that value to GetMouseHelper 
	//which returns that number of button values in a vector.      
	PsychCopyInIntegerArg(1, kPsychArgRequired, &numButtons);
	if(numButtons > 32)
		PsychErrorExitMsg(PsychErorr_argumentValueOutOfRange, "numButtons must not exceed 32");

	// Special codes -10 to -15? --> Console keyboard queries:
	if(numButtons <= -10 && numButtons >= -15) {
		ConsoleInputHelper((int) numButtons);
		return(PsychError_none);
	}

	if(numButtons < 1) 
		PsychErrorExitMsg(PsychErorr_argumentValueOutOfRange, "numButtons must exceed 1");

	doButtonArray=PsychAllocOutDoubleMatArg(3, kPsychArgOptional, (int)1, (int)numButtons, (int)1, &buttonArray);
	if(doButtonArray){
		buttonState=GetCurrentButtonState();
		for(i=0;i<numButtons;i++)
			buttonArray[i]=(double)(buttonState & (1<<i));
	}
			
	//cursor position
	GetGlobalMouse(&mouseXY);
	PsychCopyOutDoubleArg(1, kPsychArgOptional, (double)mouseXY.h);
	PsychCopyOutDoubleArg(2, kPsychArgOptional, (double)mouseXY.v);

	// Return optional keyboard input focus status:
	if (numButtons > 0) {
		// Window provided?
		if (PsychIsWindowIndexArg(2)) {
			// Yes: Check if it has focus.
			PsychAllocInWindowRecordArg(2, TRUE, &windowRecord);
			if (!PsychIsOnscreenWindow(windowRecord)) {
				PsychErrorExitMsg(PsychError_user, "Provided window handle isn't an onscreen window, as required.");
			}

			PsychCopyOutDoubleArg(4, kPsychArgOptional, (double) (GetUserFocusWindow() == windowRecord->targetSpecific.windowHandle) ? 1 : 0);
		} else {
			// No. Just always return "has focus":
			PsychCopyOutDoubleArg(4, kPsychArgOptional, (double) 1);
		}
	}

#endif

#if PSYCH_SYSTEM == PSYCH_WINDOWS
	static unsigned char disabledKeys[256];
	static unsigned char firsttime = 1;
	int keysdown, i, priorityLevel;
	unsigned char keyState[256];
	double* buttonArray;
	double numButtons, timestamp;
	PsychNativeBooleanType* buttonStates;
	POINT		point;
	HANDLE	   currentProcess;
	DWORD   oldPriority = NORMAL_PRIORITY_CLASS;
    const  DWORD   realtime_class = REALTIME_PRIORITY_CLASS;
	PsychWindowRecordType *windowRecord;

	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};

	// Retrieve optional number of mouse buttons:
	numButtons = 0;
	PsychCopyInDoubleArg(1, FALSE, &numButtons);

	// Are we operating in 'GetMouseHelper' mode? numButtons>=0 indicates this.
	if (numButtons>=0) {
		// GetMouse-Mode: Return mouse button states and mouse cursor position:

		PsychAllocOutDoubleMatArg(3, kPsychArgOptional, (int)1, (int)3, (int)1, &buttonArray);
		// Query and return mouse button state:
		PsychGetMouseButtonState(buttonArray);
		// Query and return cursor position in global coordinates:
		GetCursorPos(&point);
		PsychCopyOutDoubleArg(1, kPsychArgOptional, (double) point.x);
		PsychCopyOutDoubleArg(2, kPsychArgOptional, (double) point.y);
		
		// Window provided?
		if (PsychIsWindowIndexArg(2)) {
			// Yes: Check if it has focus.
			PsychAllocInWindowRecordArg(2, TRUE, &windowRecord);
			if (!PsychIsOnscreenWindow(windowRecord)) {
				PsychErrorExitMsg(PsychError_user, "Provided window handle isn't an onscreen window, as required.");
			}

			PsychCopyOutDoubleArg(4, kPsychArgOptional, (double) (GetForegroundWindow() == windowRecord->targetSpecific.windowHandle) ? 1 : 0);
		} else {
			// No. Just always return "has focus":
			PsychCopyOutDoubleArg(4, kPsychArgOptional, (double) 1);
		}		
	}
	else {
	  // 'KeyboardHelper' mode: We implement either KbCheck() or KbWait() via X11.
	  // This is a hack to provide keyboard queries until a PsychHID() implementation
	  // for Microsoft Windows is available...

		// Special codes -10 to -15? --> Console keyboard queries:
		if(numButtons <= -10 && numButtons >= -15) {
			ConsoleInputHelper((int) numButtons);
			return(PsychError_none);
		}
		
	  if (firsttime) {
			// First time init:
			firsttime = 0;
			memset(keyState, 0, sizeof(keyState));
			memset(disabledKeys, 0, sizeof(disabledKeys));
			// These keycodes are always disabled: 0, 255:
			disabledKeys[0]=1;
			disabledKeys[255]=1;
			// Mouse buttone (left, right, middle) are also disabled by default:
			disabledKeys[1]=1;
			disabledKeys[2]=1;
			disabledKeys[4]=1;
	  }

	  if (numButtons==-1 || numButtons==-2) {
	    // KbCheck()/KbWait() mode
	    do {
	      // Reset overall key state to "none pressed":
	      keysdown=0;

	      // Request current time of query:
	      PsychGetAdjustedPrecisionTimerSeconds(&timestamp);

			// Query state of all keys:
			for(i=1;i<255;i++){
				keyState[i] = (GetAsyncKeyState(i) & -32768) ? 1 : 0;
			}

	      // Disable all keys that are registered in disabledKeys. Check if
			// any non-disabled key is down.
	      for (i=0; i<256; i++) {
				if (disabledKeys[i]>0) keyState[i] = 0;
				keysdown+=(unsigned int) keyState[i];
	      }

	      // We repeat until any key pressed if in KbWait() mode, otherwise we
	      // exit the loop after first iteration in KbCheck mode.
	      if ((numButtons==-1) || ((numButtons==-2) && (keysdown>0))) break;

	      // Sleep for a millisecond before next KbWait loop iteration:
	      PsychWaitIntervalSeconds(0.001);

	    } while(1);

	    if (numButtons==-2) {
	      // KbWait mode: Copy out time value.
	      PsychCopyOutDoubleArg(1, kPsychArgOptional, timestamp);
	    }
	    else {
	      // KbCheck mode:
	      
	      // Copy out overall keystate:
	      PsychCopyOutDoubleArg(1, kPsychArgOptional, (keysdown>0) ? 1 : 0);

	      // Copy out timestamp:
	      PsychCopyOutDoubleArg(2, kPsychArgOptional, timestamp);	      

	      // Copy out keyboard state:
	      PsychAllocOutBooleanMatArg(3, kPsychArgOptional, 1, 256, 1, &buttonStates);

	      // Build 256 elements return vector:
	      for(i=0; i<255; i++) {
		  		buttonStates[i] = (PsychNativeBooleanType)((keyState[i+1]) ? 1 : 0);
	      }
			// Special case: Null out last element:
			buttonStates[255] = (PsychNativeBooleanType) 0;
	    }
	  }
	  
	  if (numButtons==-3) {
		// Priority() - helper mode: The 2nd argument is the priority level:

		// Determine our processID:
		currentProcess = GetCurrentProcess();
    
		// Get current scheduling policy:
		oldPriority = GetPriorityClass(currentProcess);
		
		// Map to PTB's scheme:
		switch(oldPriority) {
			case NORMAL_PRIORITY_CLASS:
				priorityLevel = 0;
			break;

			case HIGH_PRIORITY_CLASS:
				priorityLevel = 1;
			break;

			case REALTIME_PRIORITY_CLASS:
				priorityLevel = 2;
			break;

			default:
				priorityLevel = 0;
		}
        
		// Copy it out as optional return argument:
		PsychCopyOutDoubleArg(1, kPsychArgOptional, (double) priorityLevel);
		
		// Query if a new level should be set:
		priorityLevel = -1;
		PsychCopyInIntegerArg(2, kPsychArgOptional, &priorityLevel);

		// Priority level provided?
		if (priorityLevel > -1) {
			// Map to new scheduling class:
			if (priorityLevel > 2) PsychErrorExitMsg(PsychErorr_argumentValueOutOfRange, "Invalid Priority level: Requested Priority() level must not exceed 2.");

			switch(priorityLevel) {
				case 0: // Standard scheduling:
					SetPriorityClass(currentProcess, NORMAL_PRIORITY_CLASS);

					// Disable any MMCSS scheduling for us:
					PsychSetThreadPriority(0x1, 0, 0);
				break;
				
				case 1: // High priority scheduling:
					SetPriorityClass(currentProcess, HIGH_PRIORITY_CLASS);

					// Additionally try to schedule us MMCSS: This will lift us roughly into the
					// same scheduling range as REALTIME_PRIORITY_CLASS, even if we are non-admin users
					// on Vista and Windows-7 and later, however with a scheduler safety net applied.
					PsychSetThreadPriority(0x1, 10, 0);
				break;
				
				case 2: // Realtime scheduling:
					// This can fail if Matlab is not running under a user account with proper permissions:
					if ((0 == SetPriorityClass(currentProcess, REALTIME_PRIORITY_CLASS)) || (REALTIME_PRIORITY_CLASS != GetPriorityClass(currentProcess))) {
						// Failed to get RT-Scheduling. Let's try at least high priority scheduling:
						SetPriorityClass(currentProcess, HIGH_PRIORITY_CLASS);
						
						// Additionally try to schedule us MMCSS: This will lift us roughly into the
						// same scheduling range as REALTIME_PRIORITY_CLASS, even if we are non-admin users
						// on Vista and Windows-7 and later, however with a scheduler safety net applied.
						PsychSetThreadPriority(0x1, 10, 0);
					}
				break;
			}
		}
		// End of Priority() helper for Win32.
	  }
	}
#endif
	
#if PSYCH_SYSTEM == PSYCH_LINUX
	unsigned char keys_return[32];
	char* keystring;
	PsychGenericScriptType *kbNames;
	CGDirectDisplayID dpy;
	Window rootwin, childwin, mywin;
	int i, j, mx, my, dx, dy;
	double mxd, myd, dxd, dyd;
	unsigned int mask_return;
	double timestamp;
	int numButtons;
	double* buttonArray;
	PsychNativeBooleanType* buttonStates;
	int keysdown;
	XEvent event_return;
	XKeyPressedEvent keypressevent;
	int screenNumber;
	int priorityLevel;
	struct sched_param schedulingparam;
	PsychWindowRecordType *windowRecord;
	int mouseIndex;
	XIButtonState buttons_return;
	XIModifierState modifiers_return;
	XIGroupState group_return;

	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};

	PsychCopyInIntegerArg(1, kPsychArgRequired, &numButtons);

	// Retrieve optional screenNumber argument:
	if (numButtons!=-5) {
		screenNumber = 0;
		if (PsychIsScreenNumberArg(2)) {
			PsychCopyInScreenNumberArg(2, FALSE, &screenNumber);
		}

		// Map screenNumber to X11 display handle and screenid:
		PsychGetCGDisplayIDFromScreenNumber(&dpy, screenNumber);

		if (PsychIsWindowIndexArg(2)) {
			PsychAllocInWindowRecordArg(2, TRUE, &windowRecord);
			if (!PsychIsOnscreenWindow(windowRecord)) {
				PsychErrorExitMsg(PsychError_user, "Provided window handle isn't an onscreen window, as required.");
			}

			screenNumber = windowRecord->screenNumber;
			mywin = windowRecord->targetSpecific.windowHandle;

			// Map screenNumber to X11 display handle and screenid:
			PsychGetCGDisplayIDFromScreenNumber(&dpy, screenNumber);

		} else {
			mywin = RootWindow(dpy, PsychGetXScreenIdForScreen(screenNumber));
		}
	}

	// Default to "old school" mouse query - System default mouse via X core protocol:
	mouseIndex = -1;
	PsychCopyInIntegerArg(3, FALSE, &mouseIndex);

	// Are we operating in 'GetMouseHelper' mode? numButtons>=0 indicates this.
	if (numButtons>=0) {
	  // Mouse pointer query mode:
	  if (mouseIndex >= 0) {
		// XInput-2 query for handling of multiple mouse pointers:

		// Query input device list for screen:
		int nDevices;
		XIDeviceInfo* indevs = PsychGetInputDevicesForScreen(screenNumber, &nDevices);

		// Sanity check:
		if (NULL == indevs) PsychErrorExitMsg(PsychError_user, "Sorry, your system does not support individual mouse pointer queries.");
		if (mouseIndex >= nDevices) PsychErrorExitMsg(PsychError_user, "Invalid 'mouseIndex' provided. No such device.");
		if ((indevs[mouseIndex].use != XIMasterPointer) && (indevs[mouseIndex].use != XISlavePointer)) PsychErrorExitMsg(PsychError_user, "Invalid 'mouseIndex' provided. Not a pointer device.");

		// Slave pointer? If so, we requery the device info struct to retrieve updated
		// live device state:
		if (indevs[mouseIndex].use != XIMasterPointer) {
			indevs = XIQueryDevice(dpy, indevs[mouseIndex].deviceid, &numButtons);
			mouseIndex = 0;
			modifiers_return.effective = 0;
		}

		// Query real number of mouse buttons and the raw button and axis state
		// stored inside the device itself. This is done mostly because slave pointer
		// devices don't support XIQueryPointer() so we get their relevant info from the
		// XIDeviceInfo struct itself:
		numButtons = 0;
		for (i = 0; i < indevs[mouseIndex].num_classes; i++) {
			// printf("Class %i: Type %i\n", i, (int) indevs[mouseIndex].classes[i]->type);
			if (indevs[mouseIndex].classes[i]->type == XIButtonClass) {
				// Number of buttons: For all pointers.
				numButtons = ((XIButtonClassInfo*) indevs[mouseIndex].classes[i])->num_buttons;

				// Button state for slave pointers. Will get overriden for master pointers:
				buttons_return.mask = ((XIButtonClassInfo*) indevs[mouseIndex].classes[i])->state.mask;
				buttons_return.mask_len = ((XIButtonClassInfo*) indevs[mouseIndex].classes[i])->state.mask_len;
			}

			// Axis state for slave pointers. Will get overriden for master pointers:
			if (indevs[mouseIndex].classes[i]->type == XIValuatorClass) {
				XIValuatorClassInfo* axis = (XIValuatorClassInfo*) indevs[mouseIndex].classes[i];
				if (axis->number == 0) mxd = axis->value;
				if (axis->number == 1) myd = axis->value;

				// printf("AXIS %i, LABEL = %s, MIN = %f, MAX = %f, VAL = %f\n", axis->number, (char*) "NONE", (float) axis->min, (float) axis->max, (float) axis->value);
			}
		}

		// Add 32 buttons for modifier key state vector:
		numButtons += 32;

		// A real master pointer: Use official query for mouse devices.
		if (indevs[mouseIndex].use == XIMasterPointer) {
			// Query pointer location and state:
			XIQueryPointer(dpy, indevs[mouseIndex].deviceid, RootWindow(dpy, PsychGetXScreenIdForScreen(screenNumber)), &rootwin, &childwin, &mxd, &myd, &dxd, &dyd,
				       &buttons_return, &modifiers_return, &group_return);
		}

		// Copy out mouse x and y position:
		PsychCopyOutDoubleArg(1, kPsychArgOptional, mxd);
		PsychCopyOutDoubleArg(2, kPsychArgOptional, myd);

		// Copy out mouse button state:
		PsychAllocOutDoubleMatArg(3, kPsychArgOptional, (int)1, (int) numButtons, (int)1, &buttonArray);

		if (numButtons > 0) {
			// Mouse buttons:
			for (i = 0; i < numButtons - 32; i++) {
				buttonArray[i] = (double) ((buttons_return.mask[i / 8] & (1 << (i % 8))) ? 1 : 0);
			}

			// Free mask if retrieved via XIQueryPointer():
			if (indevs[mouseIndex].use == XIMasterPointer) free(buttons_return.mask);

			// Append modifier key state from associated master keyboard. Last 32 entries:
			for (i = 0; i < 32; i++) {
				buttonArray[numButtons - 32 + i] = (double) ((modifiers_return.effective & (1 << i)) ? 1 : 0);
			}
		}

		// If this indevs is a single struct that was dynamically queried from a XISlavePointer, then
		// release it:
		if ((mouseIndex == 0) && (indevs[mouseIndex].use != XIMasterPointer)) XIFreeDeviceInfo(indevs);
	  }
	  else {
		// Old school core protocol query of virtual core pointer:
		XQueryPointer(dpy, RootWindow(dpy, PsychGetXScreenIdForScreen(screenNumber)), &rootwin, &childwin, &mx, &my, &dx, &dy, &mask_return);
	  
		// Copy out mouse x and y position:
		PsychCopyOutDoubleArg(1, kPsychArgOptional, (double) mx);
		PsychCopyOutDoubleArg(2, kPsychArgOptional, (double) my);
	  
		// Copy out mouse button state:
		PsychAllocOutDoubleMatArg(3, kPsychArgOptional, (int)1, (int)numButtons, (int)1, &buttonArray);

		// Bits 8, 9 and 10 of mask_return seem to correspond to mouse buttons
		// 1, 2 and 3 of a mouse for some weird reason. Bits 0-7 describe keyboard modifier keys
		// like Alt, Ctrl, Shift, ScrollLock, NumLock, CapsLock...
		// We remap here, so the first three returned entries correspond to the mouse buttons and
		// the rest is attached behind, if requested...
	  
		// Mouse buttons: Left, Middle, Right == 0, 1, 2, aka 1,2,3 in Matlab space...
		for (i=0; i<numButtons && i<3; i++) {
			buttonArray[i] = (mask_return & (1<<(i+8))) ? 1 : 0; 
		}
		// Modifier keys 0 to 7 appended:
		for (i=3; i<numButtons && i<3+8; i++) {
			buttonArray[i] = (mask_return & (1<<(i-3))) ? 1 : 0; 
		}
		// Everything else appended:
		for (i=11; i<numButtons; i++) {
			buttonArray[i] = (mask_return & (1<<i)) ? 1 : 0; 
		}
	  }

	  // Return optional 4th argument: Focus state. Returns 1 if our window has
	  // keyboard input focus, zero otherwise:
	  XGetInputFocus(dpy, &rootwin, &i);
	  PsychCopyOutDoubleArg(4, kPsychArgOptional, (double) (rootwin == mywin) ? 1 : 0);
	}
	else {
	  // 'KeyboardHelper' mode: We implement either KbCheck() or KbWait() via X11.
	  // This is a hack to provide keyboard queries until a PsychHID() implementation
	  // for Linux is available...

		// Special codes -10 to -15? --> Console keyboard queries:
		if(numButtons <= -10 && numButtons >= -15) {
			ConsoleInputHelper((int) numButtons);
			return(PsychError_none);
		}
		
	  if (numButtons==-1 || numButtons==-2) {
	    // KbCheck()/KbWait() mode:

	    // Switch X-Server into synchronous mode: We need this to get
	    // a higher timing precision.
	    XSynchronize(dpy, TRUE);

	    do {
	      // Reset overall key state to "none pressed":
	      keysdown=0;

	      // Request current keyboard state from X-Server:
	      XQueryKeymap(dpy, keys_return);

	      // Request current time of query:
	      PsychGetAdjustedPrecisionTimerSeconds(&timestamp);

	      // Any key down?
	      for (i=0; i<32; i++) keysdown+=(unsigned int) keys_return[i];
	      
	      // We repeat until any key pressed if in KbWait() mode, otherwise we
	      // exit the loop after first iteration in KbCheck mode.
	      if ((numButtons==-1) || ((numButtons==-2) && (keysdown>0))) break;

	      // Sleep for a few milliseconds before next KbWait loop iteration:
	      PsychWaitIntervalSeconds(0.01);
	    } while(1);

	    if (numButtons==-2) {
	      // Copy out time:
	      PsychCopyOutDoubleArg(1, kPsychArgOptional, timestamp);
	    }
	    else {
	      // KbCheck mode:
	      
	      // Copy out overall keystate:
	      PsychCopyOutDoubleArg(1, kPsychArgOptional, (keysdown>0) ? 1 : 0);
	      // copy out timestamp:
	      PsychCopyOutDoubleArg(2, kPsychArgOptional, timestamp);	      
	      // Copy keyboard state:
	      PsychAllocOutBooleanMatArg(3, kPsychArgOptional, 1, 256, 1, &buttonStates);

	      // Map 32 times 8 bitvector to 256 element return vector:
	      for(i=0; i<32; i++) {
				for(j=0; j<8; j++) {
		  			buttonStates[i*8 + j] = (PsychNativeBooleanType)(keys_return[i] & (1<<j)) ? 1 : 0;
				}
	      }
	    }
	  }
	  else if (numButtons == -3) {
	    // numButtons == -3 --> KbName mapping mode:
	    // Return the full keyboard keycode to ASCII character code mapping table...
	    PsychAllocOutCellVector(1, kPsychArgOptional, 256, &kbNames);

	    for(i=0; i<256; i++) {
	      // Map keyboard scan code to KeySym:
	      keystring = XKeysymToString(XKeycodeToKeysym(dpy, i, 0));
	      if (keystring) {
		// Character found: Return its ASCII name string:
		PsychSetCellVectorStringElement(i, keystring, kbNames);
	      }
	      else {
		// No character for this keycode:
		PsychSetCellVectorStringElement(i, "", kbNames);
	      }
	    }
	  }
	  else if (numButtons == -4) {
	    // GetChar() emulation.

/* 	    do { */
/* 	      // Fetch next keypress event from queue, block if none is available... */
/* 	      keystring = NULL; */
/* 	      XNextEvent(dpy, &event_return); */
/* 	      // Check for valid keypress event and extract character: */
/* 	      if (event_return.type == KeyPress) { */
/* 		keypressevent = (XKeyPressedEvent) event_return; */
/* 		keystring = NULL; */
/* 		keystring = XKeysymToString(XKeycodeToKeysym(dpy, keypressevent.keycode, 0)); */
/* 	      } */
/* 	      // Repeat until a valid char is returned. */
/* 	    } while (keystring == NULL); */

/* 	    // Copy out character: */
/* 	    PsychCopyOutCharArg(1, kPsychArgOptional, (char) keystring); */
/* 	    // Copy out time: */
/* 	    PsychCopyOutDoubleArg(2, kPsychArgOptional, (double) keypressevent.time); */
	  }
	  else if (numButtons==-5) {
		// Priority() - helper mode: The 2nd argument is the priority level:

		// Query RT priority level (if RT scheduling active):
		sched_getparam(0, &schedulingparam);
		// Query scheduling mode:
		priorityLevel = sched_getscheduler(0);
		// If scheduling mode is a realtime mode (RoundRobin realtime RR, or FIFO realtime),
		// then assign RT priority level (range 1-99) as current priorityLevel, otherwise
		// assign non realtime priority level zero:
		priorityLevel = (priorityLevel == SCHED_RR || priorityLevel == SCHED_FIFO) ? schedulingparam.sched_priority : 0;
        
		// Copy it out as optional return argument:
		PsychCopyOutDoubleArg(1, kPsychArgOptional, (double) priorityLevel);
		
		// Query if a new level should be set:
		priorityLevel = -1;
		PsychCopyInIntegerArg(2, kPsychArgOptional, &priorityLevel);

		errno=0;
		// Priority level provided?
		if (priorityLevel > -1) {
			// Map to new scheduling class:
			if (priorityLevel > 99 || priorityLevel < 0) PsychErrorExitMsg(PsychErorr_argumentValueOutOfRange, "Invalid Priority level: Requested Priority() level must be between zero and 99!");

			if (priorityLevel > 0) {
				// Realtime FIFO scheduling and all pages of Matlab/Octave locked into memory:
				schedulingparam.sched_priority = priorityLevel;
				priorityLevel = sched_setscheduler(0, SCHED_FIFO, &schedulingparam);
				if (priorityLevel == -1) {
					// Failed!
					if(!PsychPrefStateGet_SuppressAllWarnings()) {
	    					printf("PTB-ERROR: Failed to enable realtime-scheduling with Priority(%i) [%s]!\n", schedulingparam.sched_priority, strerror(errno));
						if (errno==EPERM) {
							printf("PTB-ERROR: You need to run Matlab/Octave with root-privileges for this to work.\n");
						}
					}
					errno=0;
				}
				else {
					// RT-Scheduling active. Lock all current and future memory:
					priorityLevel = mlockall(MCL_CURRENT | MCL_FUTURE);
					if (priorityLevel!=0) {
						// Failed! Report problem as warning, but don't worry further. 
	    					if(!PsychPrefStateGet_SuppressAllWarnings()) printf("PTB-WARNING: Failed to enable system memory locking with Priority(%i) [%s]!\n", schedulingparam.sched_priority, strerror(errno));
						// Undo any possibly partial mlocks....
						munlockall();
						errno=0;
					}
				}
			}
			else {
				// Standard scheduling and no memory locking:
				schedulingparam.sched_priority = 0;
				priorityLevel = sched_setscheduler(0, SCHED_OTHER, &schedulingparam);
				if (priorityLevel == -1) {
					// Failed!
					if(!PsychPrefStateGet_SuppressAllWarnings()) {
	    					printf("PTB-ERROR: Failed to disable realtime-scheduling with Priority(%i) [%s]!\n", schedulingparam.sched_priority, strerror(errno));
						if (errno==EPERM) {
							printf("PTB-ERROR: You need to run Matlab/Octave with root-privileges for this to work.\n");
						}
					}
					errno=0;
				}

				munlockall();
				errno=0;
			}
			// End of setup of new Priority...
		}
		// End of Priority() helper for Linux.
	  }
	}	// End of special functions handling for Linux...
#endif
	return(PsychError_none);	
}
