/*
	PsychtoolboxGL/Source/Common/PsychHID/PsychHIDKbQueueCreate.c		
  
	PROJECTS: 
	
		PsychHID only.
  
	PLATFORMS:  
	
		All.  
  
	AUTHORS:
	
		rwoods@ucla.edu		rpw 
        mario.kleiner@tuebingen.mpg.de      mk
      
	HISTORY:
		8/19/07  rpw		Created.
		8/23/07  rpw        Added PsychHIDQueueFlush to documentation
		12/17/09 rpw		Added support for keypads
  
	NOTES:
	
		The routines PsychHIDKbQueueCreate, PsychHIDKbQueueStart, PsychHIDKbQueueCheck, PsychHIDKbQueueStop
		and PsychHIDKbQueueRelease comprise a replacement for PsychHIDKbCheck, providing the following
		advantages:
		
			1) Brief key presses that would be missed by PsychHIDKbCheck are reliably detected
			2) The times of key presses are recorded more accurately
			3) Key releases are also recorded
		
		Requires Mac OS X 10.3 or later. The Matlab wrapper functions (KbQueueCreate, KbQueueStart,
		KbQueueCheck, KbQueueStop and KbQueueRelease screen away Mac OS X 10.2 and earlier, and the C code 
		does nothing to verify the Mac OS X version.
		
		Only a single device can be monitored at any given time. The deviceNumber can be specified only
		in the call to PsychHIDKbQueueCreate. The other routines then relate to that specified device. If
		deviceNumber is not specified, the first device is the default (like PyschHIDKbCheck). If
		PsychHIDKbQueueCreate has not been called first, the other routines will generate an error 
		message. Likewise, if PsychHIDKbQueueRelease has been called more recently than PsychHIDKbQueueCreate,
		the other routines will generate error messages.
		
		It is acceptable to cal PsychHIDKbQueueCreate at any time (e.g., to switch to a new device) without
		calling PsychKbQueueRelease.
		
		PsychHIDKbQueueCreate:
			Creates the queue for the specified (or default) device number
			No events are delivered to the queue until PsychHIDKbQueueStart is called
			Can be called again at any time
			
		PsychHIDKbQueueStart:
			Starts delivering keyboard or keypad events from the specified device to the queue
			
		PsychHIDKbQueueStop:
			Stops delivery of new keyboard or keypad events from the specified device to the queue.
			Data regarding events already queued is not cleared and can be recovered by PsychHIDKbQueueCheck
			
		PsychHIDKbQueueCheck:
			Obtains data about keypresses on the specified device since the most recent call to
			this routine or to PsychHIDKbQueueStart
			
			Clears all currently scored events (unscored events may still be in the queue)
			
		PsychHIDKbQueueFlush:
			Flushes unscored events from the queue and zeros all previously scored events
			
		PsychHIDKbQueueRelease:
			Releases queue-associated resources; once called, PsychHIDKbQueueCreate must be invoked
			before using any of the other routines
			
			This routine is called automatically at clean-up and can be omitted at the potential expense of
			keeping memory allocated unnecesarily
				

		---

*/

#include "PsychHID.h"

static char useString[]= "PsychHID('KbQueueCreate', [deviceNumber], [keyFlags])";
static char synopsisString[] = 
        "Creates a queue for events generated by an input device (keyboard, keypad, mouse, ...).\n"
        "By default the first keyboard device (the one with the lowest device number) is "
        "used. If no keyboard is found, the first keypad device is used, followed by other "
        "devices, e.g., mice.  Optionally, the deviceNumber of any keyboard or HID device may be specified.\n"
        "On OS/X only one input device queue is allowed at a time.\n"
        "On MS-Windows XP and later, it is currently not possible to enumerate different keyboards and mice "
        "separately. Therefore the 'deviceNumber' argument is mostly useless for keyboards and mice. Usually you can "
        "only check the system keyboard or mouse.\n";

static char seeAlsoString[] = "KbQueueStart, KbQueueStop, KbQueueCheck, KbQueueFlush, KbQueueRelease";

PsychError PSYCHHIDKbQueueCreate(void) 
{
	int deviceIndex = -1;
	int numScankeys = 0;
	int* scanKeys = NULL;
	int rc;
	
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};

	PsychErrorExit(PsychCapNumInputArgs(2)); // Optionally specifies the deviceNumber of the keyboard or keypad to scan and an array of keyCode indicators.

	// Get optional deviceIndex:
	PsychCopyInIntegerArg(1, FALSE, &deviceIndex);

	// Get optional scanKeys vector:
	PsychAllocInIntegerListArg(2, FALSE, &numScankeys, &scanKeys);

	// Perform actual, OS-dependent init and return its status code:
	rc = PsychHIDOSKbQueueCreate(deviceIndex, numScankeys, scanKeys);

	return(rc);
}


#if PSYCH_SYSTEM == PSYCH_OSX

#include "PsychHIDKbQueue.h"
#include <errno.h>

#define NUMDEVICEUSAGES 7

// Declare globally scoped variables that will be declared extern by other functions in this family
AbsoluteTime *psychHIDKbQueueFirstPress=NULL;
AbsoluteTime *psychHIDKbQueueFirstRelease=NULL;
AbsoluteTime *psychHIDKbQueueLastPress=NULL;
AbsoluteTime *psychHIDKbQueueLastRelease=NULL;
HIDDataRef hidDataRef=NULL;
pthread_mutex_t psychHIDKbQueueMutex;
CFRunLoopRef psychHIDKbQueueCFRunLoopRef=NULL;
pthread_t psychHIDKbQueueThread = NULL;
psych_bool queueIsAKeyboard;

static void *PsychHIDKbQueueNewThread(void *value){
	// The new thread is started after the global variables are initialized
	SInt32 rc;

	// Get and retain the run loop associated with this thread
	psychHIDKbQueueCFRunLoopRef=(CFRunLoopRef) GetCFRunLoopFromEventLoop(GetCurrentEventLoop());
	CFRetain(psychHIDKbQueueCFRunLoopRef);

	// Put the event source into the run loop
	if(!CFRunLoopContainsSource(psychHIDKbQueueCFRunLoopRef, hidDataRef->eventSource, kCFRunLoopDefaultMode))
		CFRunLoopAddSource(psychHIDKbQueueCFRunLoopRef, hidDataRef->eventSource, kCFRunLoopDefaultMode);

	// Switch ourselves (NULL) to RT scheduling: We promise to use / require at most (0+1) == 1 msec every
	// 10 msecs and allow for wakeup delay/jitter of up to 2 msecs -- perfectly reasonable, given that we
	// only do minimal << 1 msec processing, only at the timescale of human reaction times, and driven by
	// input devices with at least 4+/-4 msecs jitter at 8 msec USB polling frequency.
	PsychSetThreadPriority(NULL, 2, 0);

	// Start the run loop, code execution will block here until run loop is stopped again by PsychHIDKbQueueRelease
	// Meanwhile, the run loop of this thread will be responsible for executing code below in PsychHIDKbQueueCalbackFunction
	while ((rc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false)) == kCFRunLoopRunTimedOut);
		
	// In case the CFRunLoop was interrupted while the mutex was locked, unlock it
	pthread_mutex_unlock(&psychHIDKbQueueMutex);	
}

static double convertTime(AbsoluteTime at){
	Nanoseconds timeNanoseconds=AbsoluteToNanoseconds(at);
	UInt64 timeUInt64=UnsignedWideToUInt64(timeNanoseconds);
	double timeDouble=(double)timeUInt64;
	return timeDouble / 1000000000;
}

/*	Table mapping HID usages in the KeyboardOrKeypad page to virtual key codes.
	Names given for keys refer to US layout.
	May be copied freely.
*/
// Taken from this MIT licensed software: <https://github.com/Ahruman/KeyNaming> according
// to above permission note. So far not effectively used. Probably need to pull in the
// whole project...
/*  KeyNaming.cp
	Keynaming 2.2 implementation
	� 2001-2008 Jens Ayton <jens@ayton.se>, except where otherwise noted.
	
	Copyright � 2001�2008 Jens Ayton
*/

#define kVKC_Unknown		0xFFFF

enum
{
	/*	Virtual key codes handled specially (from: Inside Macintosh: Text, Appendix C)
		
		Function keys. These are handled by loading the releveant string IFF the
		character code 10 (kCC_FKey) is generated. This allows the function keys
		to be remapped.
		
		The Menu/Application key on Windows keyboards also generates the character
		code 10 by default. It isn't immediately clear whether the VKC is used for
		an F key in other circumstances, but apps generally seem to interpret it as
		a normal key. For now, I'm defining it as Menu Key. It's possible Unix-
		oriented keyboards use it with a different key cap, e.g. Meta.
	*/
	kVKC_F1				= 122,
	kVKC_F2				= 120,
	kVKC_F3				= 99,
	kVKC_F4				= 118,
	kVKC_F5				= 96,
	kVKC_F6				= 97,
	kVKC_F7				= 98,
	kVKC_F8				= 100,
	kVKC_F9				= 101,
	kVKC_F10			= 109,
	kVKC_F11			= 103,
	kVKC_F12			= 111,
	kVKC_F13			= 105,
	kVKC_F14			= 107,
	kVKC_F15			= 113,
	kVKC_F16			= 106,
	kVKC_Menu			= 110,

	/*	Escape and clear are like the F keys, using the character code kCC_EscClr. */
	kVKC_Esc			= 53,
	kVKC_Clear			= 71,

	/*	The following are handled directy by recognising the virtual code. */
	kVKC_Space			= 49,
	kVKC_CapsLock		= 57,
	kVKC_Shift			= 56,
	kVKC_Option			= 58,
	kVKC_Control		= 59,
	kVKC_rShift			= 60,		/*	Right-hand modifiers; not implemented */
	kVKC_rOption		= 61,
	kVKC_rControl		= 62,
	kVKC_Command		= 55,
	kVKC_Return			= 36,
	kVKC_Backspace		= 51,		/*	Left delete */
	kVKC_Delete			= 117,		/*	right delete */
	kVKC_Help			= 114,
	kVKC_Home			= 115,
	kVKC_PgUp			= 116,
	kVKC_PgDn			= 121,
	kVKC_End			= 119,
	kVKC_LArrow			= 123,
	kVKC_RArrow			= 124,
	kVKC_UArrow			= 126,
	kVKC_DArrow			= 125,
	kVKC_KpdEnter		= 76,		/*	"normal" enter key */
	kVKC_KbdEnter		= 52,		/*	Powerbooks (and some early Macs) */
	kVKC_Fn				= 63,

	/*	Keypad keys. These are named by loading the string "Keypad %@" and
		replacing the %@ with the key's character name. The constant names
		do not correspond to the key caps in any KCHR that I'm aware of;
		they're just used to recognise the set of keys. Note that Enter and
		Clear aren't handled this way. */
	kVKC_Kpd_0			= 81,
	kVKC_Kpd_1			= 75,
	kVKC_Kpd_2			= 67,
	kVKC_Kpd_3			= 89,
	kVKC_Kpd_4			= 91,
	kVKC_Kpd_5			= 92,
	kVKC_Kpd_6			= 78,
	kVKC_Kpd_7			= 86,
	kVKC_Kpd_8			= 87,
	kVKC_Kpd_9			= 88,
	kVKC_Kpd_A			= 69,
	kVKC_Kpd_B			= 83,
	kVKC_Kpd_C			= 84,
	kVKC_Kpd_D			= 85,
	kVKC_Kpd_E			= 82,
	kVKC_Kpd_F			= 65,

	/* 2.1b5: values from new list in Event.h in OS X 10.5 */
	kVKC_VolumeUp		= 72,
	kVKC_VolumeDown		= 73,
	kVKC_Mute			= 74,

	kVKC_F17			= 64,
	kVKC_F18			= 79,
	kVKC_F19			= 80,
	kVKC_F20			= 90,

	#if KEYNAMING_ENABLE_HID

	/*	Fake VKCs. These are used for HID usages that, to my knowledge, have
		no corresponding VKC. I use the range 0x6000 up; GetKeys() can't return
		in this range, and other VKC usages probably won't. */
	kFKC_base_			= 0x6000,
	kFKC_rCommand,
/*	kFKC_Mute,			*/
/*	kFKC_VolumeDown,	*/
/*	kFKC_VolumeUp,		*/
	kFKC_Power			= 0x6005,
	kFKC_Eject,
/*	kFKC_F17,			*/
/*	kFKC_F18,			*/
/*	kFKC_F19,			*/
/*	kFKC_F20,			*/
	kFKC_F21			= 0x600B,
	kFKC_F22,
	kFKC_F23,
	kFKC_F24,

	#endif

	kEnumAtTheEndToSatisfyIrritableCompilers
};

static const uint16_t	kHID2VKC[] =
{
	kVKC_Unknown,		/* Reserved (no event indicated) */
	kVKC_Unknown,		/* ErrorRollOver */
	kVKC_Unknown,		/* POSTFail */
	kVKC_Unknown,		/* ErrorUndefined */
	0x00,				/* a and A */
	0x0B,				/* b and B */
	0x08,				/* ... */
	0x02,
	0x0E,
	0x03,
	0x05,
	0x04,
	0x22,
	0x26,
	0x28,
	0x25,
	0x2E,
	0x2D,
	0x1F,
	0x23,
	0x0C,
	0x0F,
	0x01,
	0x11,
	0x20,
	0x09,
	0x0D,
	0x07,
	0x10,
	0x06,				/* z and Z */
	0x12,				/* 1 */
	0x13,				/* 2 */
	0x14,				/* ... */
	0x15,
	0x17,
	0x16,
	0x1A,
	0x1C,
	0x19,				/* 9 */
	0x1D,				/* 0 */
	kVKC_Return,		/* Keyboard Return (ENTER) */
	kVKC_Esc,			/* Escape */
	kVKC_Backspace,		/* Delete (Backspace) */
	0x30,				/* Tab */
	kVKC_Space,			/* Space bar */
	0x1B,				/* - and _ */
	0x18,				/* = and + */
	0x21,				/* [ and { */
	0x1E,				/* ] and } */
	0x2A,				/* \ and | */
	kVKC_Unknown,		/* "Non-US # and ~" ?? */
	0x29,				/* ; and : */
	0x27,				/* ' and " */
	0x32,				/* ` and ~ */
	0x2B,				/* , and < */
	0x2F,				/* . and > */
	0x2C,				/* / and ? */
	kVKC_CapsLock,		/* Caps Lock */
	kVKC_F1,			/* F1 */
	kVKC_F2,			/* ... */
	kVKC_F3,
	kVKC_F4,
	kVKC_F5,
	kVKC_F6,
	kVKC_F7,
	kVKC_F8,
	kVKC_F9,
	kVKC_F10,
	kVKC_F11,
	kVKC_F12,			/* F12 */
	kVKC_Unknown,		/* Print Screen */
	kVKC_Unknown,		/* Scroll Lock */
	kVKC_Unknown,		/* Pause */
	kVKC_Unknown,		/* Insert */
	kVKC_Home,			/* Home */
	kVKC_PgUp,			/* Page Up */
	kVKC_Delete,		/* Delete Forward */
	kVKC_End,			/* End */
	kVKC_PgDn,			/* Page Down */
	kVKC_RArrow,		/* Right Arrow */
	kVKC_LArrow,		/* Left Arrow */
	kVKC_DArrow,		/* Down Arrow */
	kVKC_UArrow,		/* Up Arrow */
	kVKC_Clear,			/* Keypad Num Lock and Clear */
	0x4B,				/* Keypad / */
	0x43,				/* Keypad * */
	0x4E,				/* Keypad - */
	0x45,				/* Keypad + */
	kVKC_KpdEnter,		/* Keypad ENTER */
	0x53,				/* Keypad 1 */
	0x54,				/* Keypad 2 */
	0x55,				/* Keypad 3 */
	0x56,				/* Keypad 4 */
	0x57,				/* Keypad 5 */
	0x58,				/* Keypad 6 */
	0x59,				/* Keypad 7 */
	0x5B,				/* Keypad 8 */
	0x5C,				/* Keypad 9 */
	0x52,				/* Keypad 0 */
	0x41,				/* Keypad . */
	0x32,				/* "Keyboard Non-US \ and |" */
	kVKC_Unknown,		/* "Keyboard Application" (Windows key for Windows 95, and "Compose".) */
	kVKC_Unknown,		/* Keyboard Power (status, not key... but Apple doesn't seem to have read the spec properly) */
	0x51,				/* Keypad = */
	kVKC_F13,			/* F13 */
	kVKC_F14,			/* ... */
	kVKC_F15,
	kVKC_F16,
	kVKC_F17,
	kVKC_F18,
	kVKC_F19,
	kVKC_F20,
	kVKC_Unknown,
	kVKC_Unknown,
	kVKC_Unknown,
	kVKC_Unknown,			/* F24 */
	kVKC_Unknown,		/* Keyboard Execute */
	kVKC_Help,			/* Keyboard Help */
	kVKC_Unknown,		/* Keyboard Menu */
	kVKC_Unknown,		/* Keyboard Select */
	kVKC_Unknown,		/* Keyboard Stop */
	kVKC_Unknown,		/* Keyboard Again */
	kVKC_Unknown,		/* Keyboard Undo */
	kVKC_Unknown,		/* Keyboard Cut */
	kVKC_Unknown,		/* Keyboard Copy */
	kVKC_Unknown,		/* Keyboard Paste */
	kVKC_Unknown,		/* Keyboard Find */
	kVKC_Mute,			/* Keyboard Mute */
	kVKC_VolumeUp,		/* Keyboard Volume Up */
	kVKC_VolumeDown,	/* Keyboard Volume Down */
	kVKC_CapsLock,		/* Keyboard Locking Caps Lock */
	kVKC_Unknown,		/* Keyboard Locking Num Lock */
	kVKC_Unknown,		/* Keyboard Locking Scroll Lock */
	0x41,				/*	Keypad Comma ("Keypad Comma is the appropriate usage for the Brazilian
							keypad period (.) key. This represents the closest possible  match, and
							system software should do the correct mapping based on the current locale
							setting." If strange stuff happens on a (physical) Brazilian keyboard,
							I'd like to know about it. */
	0x51,				/* Keypad Equal Sign ("Used on AS/400 Keyboards.") */
	kVKC_Unknown,		/* Keyboard International1 (Brazilian / and ? key? Kanji?) */
	kVKC_Unknown,		/* Keyboard International2 (Kanji?) */
	kVKC_Unknown,		/* Keyboard International3 (Kanji?) */
	kVKC_Unknown,		/* Keyboard International4 (Kanji?) */
	kVKC_Unknown,		/* Keyboard International5 (Kanji?) */
	kVKC_Unknown,		/* Keyboard International6 (Kanji?) */
	kVKC_Unknown,		/* Keyboard International7 (Kanji?) */
	kVKC_Unknown,		/* Keyboard International8 (Kanji?) */
	kVKC_Unknown,		/* Keyboard International9 (Kanji?) */
	kVKC_Unknown,		/* Keyboard LANG1 (Hangul/English toggle) */
	kVKC_Unknown,		/* Keyboard LANG2 (Hanja conversion key) */
	kVKC_Unknown,		/* Keyboard LANG3 (Katakana key) */		// kVKC_Kana?
	kVKC_Unknown,		/* Keyboard LANG4 (Hirigana key) */
	kVKC_Unknown,		/* Keyboard LANG5 (Zenkaku/Hankaku key) */
	kVKC_Unknown,		/* Keyboard LANG6 */
	kVKC_Unknown,		/* Keyboard LANG7 */
	kVKC_Unknown,		/* Keyboard LANG8 */
	kVKC_Unknown,		/* Keyboard LANG9 */
	kVKC_Unknown,		/* Keyboard Alternate Erase ("Example, Erase-Eaze� key.") */
	kVKC_Unknown,		/* Keyboard SysReq/Attention */
	kVKC_Unknown,		/* Keyboard Cancel */
	kVKC_Unknown,		/* Keyboard Clear */
	kVKC_Unknown,		/* Keyboard Prior */
	kVKC_Unknown,		/* Keyboard Return */
	kVKC_Unknown,		/* Keyboard Separator */
	kVKC_Unknown,		/* Keyboard Out */
	kVKC_Unknown,		/* Keyboard Oper */
	kVKC_Unknown,		/* Keyboard Clear/Again */
	kVKC_Unknown,		/* Keyboard CrSel/Props */
	kVKC_Unknown,		/* Keyboard ExSel */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Keypad 00 */
	kVKC_Unknown,		/* Keypad 000 */
	kVKC_Unknown,		/* Thousands Separator */
	kVKC_Unknown,		/* Decimal Separator */
	kVKC_Unknown,		/* Currency Unit */
	kVKC_Unknown,		/* Currency Sub-unit */
	kVKC_Unknown,		/* Keypad ( */
	kVKC_Unknown,		/* Keypad ) */
	kVKC_Unknown,		/* Keypad { */
	kVKC_Unknown,		/* Keypad } */
	kVKC_Unknown,		/* Keypad Tab */
	kVKC_Unknown,		/* Keypad Backspace */
	kVKC_Unknown,		/* Keypad A */
	kVKC_Unknown,		/* Keypad B */
	kVKC_Unknown,		/* Keypad C */
	kVKC_Unknown,		/* Keypad D */
	kVKC_Unknown,		/* Keypad E */
	kVKC_Unknown,		/* Keypad F */
	kVKC_Unknown,		/* Keypad XOR */
	kVKC_Unknown,		/* Keypad ^ */
	kVKC_Unknown,		/* Keypad % */
	kVKC_Unknown,		/* Keypad < */
	kVKC_Unknown,		/* Keypad > */
	kVKC_Unknown,		/* Keypad & */
	kVKC_Unknown,		/* Keypad && */
	kVKC_Unknown,		/* Keypad | */
	kVKC_Unknown,		/* Keypad || */
	kVKC_Unknown,		/* Keypad : */
	kVKC_Unknown,		/* Keypad # */
	kVKC_Unknown,		/* Keypad Space */
	kVKC_Unknown,		/* Keypad @ */
	kVKC_Unknown,		/* Keypad ! */
	kVKC_Unknown,		/* Keypad Memory Store */
	kVKC_Unknown,		/* Keypad Memory Recall */
	kVKC_Unknown,		/* Keypad Memory Clear */
	kVKC_Unknown,		/* Keypad Memory Add */
	kVKC_Unknown,		/* Keypad Memory Subtract */
	kVKC_Unknown,		/* Keypad Memory Multiply */
	kVKC_Unknown,		/* Keypad Memory Divide */
	kVKC_Unknown,		/* Keypad +/- */
	kVKC_Unknown,		/* Keypad Clear */
	kVKC_Unknown,		/* Keypad Clear Entry */
	kVKC_Unknown,		/* Keypad Binary */
	kVKC_Unknown,		/* Keypad Octal */
	kVKC_Unknown,		/* Keypad Decimal */
	kVKC_Unknown,		/* Keypad Hexadecimal */
	kVKC_Unknown,		/* Reserved */
	kVKC_Unknown,		/* Reserved */
	kVKC_Control,		/* Keyboard LeftControl */
	kVKC_Shift,			/* Keyboard LeftShift */
	kVKC_Option,		/* Keyboard LeftAlt */
	kVKC_Command,		/* Keyboard LeftGUI */
	kVKC_rControl,		/* Keyboard RightControl */
	kVKC_rShift,		/* Keyboard RightShift */
	kVKC_rOption,		/* Keyboard RightAlt */
	kVKC_Unknown		/* Keyboard RightGUI */
};
enum { kHID2VKCSize = sizeof kHID2VKC / sizeof kHID2VKC[0] };


static void PsychHIDKbQueueCallbackFunction(void *target, IOReturn result, void *refcon, void *sender)
{
	// This routine is executed each time the queue transitions from empty to non-empty
	// The CFRunLoop of the thread in PsychHIDKbQueueNewThread is the thread that executes here
	HIDDataRef hidDataRef=(HIDDataRef)refcon;
	long keysUsage=-1;
	IOHIDEventStruct event;
	AbsoluteTime zeroTime= {0,0};
	PsychHIDEventRecord evt;
	
	result=kIOReturnError;
	if(!hidDataRef) return;	// Nothing we can do because we can't access queue, (shouldn't happen)
	
	while(1){
		// This function only gets called when queue transitions from empty to non-empty
		// Therefore, we must process all available events in this while loop before
		// it will be possible for this function to be notified again
		{
			// Get next event from queue
			result = (*hidDataRef->hidQueueInterface)->getNextEvent(hidDataRef->hidQueueInterface, &event, zeroTime, 0);
			if(kIOReturnSuccess!=result) return;
		}
		if ((event.longValueSize != 0) && (event.longValue != NULL)) free(event.longValue);
		{
			// Get element associated with event so we can get its usage page
			CFMutableDataRef element=NULL;
			{
				CFNumberRef number= CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &event.elementCookie);
				if (!number)  continue;
				element = (CFMutableDataRef)CFDictionaryGetValue(hidDataRef->hidElementDictionary, number);
				CFRelease(number);
			}
			if (!element) continue;
			{
				HIDElementRef tempHIDElement=(HIDElement *)CFDataGetMutableBytePtr(element);
				if(!tempHIDElement) continue;
				keysUsage=tempHIDElement->usage;
			}
		}
		// if(keysUsage<1 || keysUsage>255) continue;	// This is redundant since usage is checked when elements are added

		// Don't bother with keysUsage of 0 (meaningless) or 1 (ErrorRollOver) for keyboards:
		if ((queueIsAKeyboard) && (keysUsage <= 1)) continue;
		
		// Clear ringbuffer event:
		memset(&evt, 0 , sizeof(evt));

		// Cooked key code defaults to "unhandled", and stays that way for anything but keyboards:
		evt.cookedEventCode = -1;
		
		// For real keyboards we can compute cooked key codes:
		if (queueIsAKeyboard) {
			// Keyboard(ish) device. We can handle this under some conditions.
			// Init to a default of handled, but unmappable/ignored keycode:
			evt.cookedEventCode = 0;

			// Keypress event? And available in mapping table?
			if ((event.value != 0) && (keysUsage < kHID2VKCSize)) {
				// Yes: We try to map this to a character code:
				
				// Step 1: Map HID usage value to virtual keycode via LUT:
				uint16_t vcKey = kHID2VKC[keysUsage];
				
				// Step 2: Translate virtual key code into unicode char:
				// Ok, this is the usual horrifying complexity of Apple's system.
				// If we want this implemented, our best shot is using/including a modified
				// version of this MIT licensed software: <https://github.com/Ahruman/KeyNaming>
				//
				// For now, i just need a break - doing some enjoyable work on a less disgusting os...
				evt.cookedEventCode = (int) vcKey;
			}
		}
		
		pthread_mutex_lock(&psychHIDKbQueueMutex);

		// Update records of first and latest key presses and releases
		if(event.value!=0){
			if(psychHIDKbQueueFirstPress){
				// First key press timestamp
				if(psychHIDKbQueueFirstPress[keysUsage-1].hi==0 && psychHIDKbQueueFirstPress[keysUsage-1].lo==0){
					psychHIDKbQueueFirstPress[keysUsage-1]=event.timestamp;
				}
			}
			if(psychHIDKbQueueLastPress){
				// Last key press timestamp
				psychHIDKbQueueLastPress[keysUsage-1]=event.timestamp;
			}
			evt.status |= (1 << 0);
		}
		else{
			if(psychHIDKbQueueFirstRelease){
				// First key release timestamp
				if(psychHIDKbQueueFirstRelease[keysUsage-1].hi==0 && psychHIDKbQueueFirstRelease[keysUsage-1].lo==0) psychHIDKbQueueFirstRelease[keysUsage-1]=event.timestamp;
			}
			if(psychHIDKbQueueLastRelease){
				// Last key release timestamp
				psychHIDKbQueueLastRelease[keysUsage-1]=event.timestamp;
			}
			evt.status &= ~(1 << 0);
		}
		
		// Update event buffer:
		evt.timestamp = convertTime(event.timestamp);
		evt.rawEventCode = keysUsage;
		PsychHIDAddEventToEventBuffer(0, &evt);
		pthread_mutex_unlock(&psychHIDKbQueueMutex);
	}
}

PsychError PsychHIDOSKbQueueCreate(int deviceIndex, int numScankeys, int* scanKeys)
{
	pRecDevice			deviceRecord;
	int					*psychHIDKbQueueKeyList;
	long				KbDeviceUsagePages[NUMDEVICEUSAGES]= {kHIDPage_GenericDesktop, kHIDPage_GenericDesktop, kHIDPage_GenericDesktop, kHIDPage_GenericDesktop, kHIDPage_GenericDesktop, kHIDPage_GenericDesktop, kHIDPage_GenericDesktop};
	long				KbDeviceUsages[NUMDEVICEUSAGES]={kHIDUsage_GD_Keyboard, kHIDUsage_GD_Keypad, kHIDUsage_GD_Mouse, kHIDUsage_GD_Pointer, kHIDUsage_GD_Joystick, kHIDUsage_GD_GamePad, kHIDUsage_GD_MultiAxisController};
	int					numDeviceUsages=NUMDEVICEUSAGES;
	
	HRESULT result;
	IOHIDDeviceInterface122** interface=NULL;	// This requires Mac OS X 10.3 or higher

	if(scanKeys && (numScankeys != 256)) {
		PsychErrorExitMsg(PsychError_user, "Second argument to KbQueueCreate must be a vector with 256 elements.");
	}

    psychHIDKbQueueKeyList = scanKeys;
    
	PsychHIDVerifyInit();
	
	if(psychHIDKbQueueCFRunLoopRef || hidDataRef || psychHIDKbQueueFirstPress || psychHIDKbQueueFirstRelease || psychHIDKbQueueLastPress || psychHIDKbQueueLastRelease){
		// We are reinitializing, so need to release prior initialization
		PsychHIDOSKbQueueRelease(deviceIndex);
	}

	// Mark as a non-keyboard device, to start with:
	queueIsAKeyboard = FALSE;

	// Find the requested device record
	{
		int deviceIndices[PSYCH_HID_MAX_KEYBOARD_DEVICES]; 
		pRecDevice deviceRecords[PSYCH_HID_MAX_KEYBOARD_DEVICES];
		int numDeviceIndices;
		int i;
		PsychHIDGetDeviceListByUsages(numDeviceUsages, KbDeviceUsagePages, KbDeviceUsages, &numDeviceIndices, deviceIndices, deviceRecords);  
		{
			// A negative device number causes the default device to be used:
			psych_bool isDeviceSpecified = (deviceIndex >= 0) ? TRUE : FALSE;

			if(isDeviceSpecified){
				psych_bool foundUserSpecifiedDevice;
				//make sure that the device number provided by the user is really a keyboard or keypad.
				for(i=0;i<numDeviceIndices;i++){
					if(foundUserSpecifiedDevice=(deviceIndices[i]==deviceIndex))
						break;
				}
				if(!foundUserSpecifiedDevice)
					PsychErrorExitMsg(PsychError_user, "Specified device number is not a keyboard or keypad device.");
			}
			else{ 
				// set the keyboard or keypad device to be the first keyboard device or, if no keyboard, the first keypad
				i=0;
				if(numDeviceIndices==0)
					PsychErrorExitMsg(PsychError_user, "No keyboard or keypad devices detected.");
			}
		}
		deviceRecord=deviceRecords[i]; 
	}

	// The queue key list is already assigned, if present at all, ie. non-NULL.
	// The list is a vector of 256 doubles, analogous to the outputs of KbQueueCheck
	// (or KbCheck) with each element of the vector corresponding to a particular
	// key. If the double in a particular position is zero, that key is not added
	// to the queue and events from that key will not be detected.
	// Note that psychHIDKbQueueList does not need to be freed because it is allocated
	// within PsychAllocInIntegerListArg using mxMalloc

	interface=deviceRecord->interface;
	if(!interface)
		PsychErrorExitMsg(PsychError_system, "Could not get interface to device.");
		
	hidDataRef=malloc(sizeof(HIDData));
	if(!hidDataRef)
		PsychErrorExitMsg(PsychError_system, "Could not allocate memory for queue.");
	bzero(hidDataRef, sizeof(HIDData));
	
	// Allocate for the queue
	hidDataRef->hidQueueInterface=(*interface)->allocQueue(interface);
	if(!hidDataRef->hidQueueInterface){
		free(hidDataRef);
		hidDataRef=NULL;
		PsychErrorExitMsg(PsychError_system, "Failed to allocate event queue for detecting key press.");
	}
	hidDataRef->hidDeviceInterface=interface;
	
	// Create the queue
	result = (*hidDataRef->hidQueueInterface)->create(hidDataRef->hidQueueInterface, 0, 30);	// The second number is the number of events can be stored before events are lost
												// Empirically, the lost events are the later events, despite my having seen claims to the contrary
												// Also, empirically, I get 11 events when specifying 8 ???
	if (kIOReturnSuccess != result){
		(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
		free(hidDataRef);
		hidDataRef=NULL;
		PsychErrorExitMsg(PsychError_system, "Failed to create event queue for detecting key press.");
	}
	{
		// Prepare dictionary then add appropriate device elements to dictionary and queue
		CFArrayRef elements;
		CFMutableDictionaryRef hidElements = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if(!hidElements){
			(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
			(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
			free(hidDataRef);
			hidDataRef=NULL;
			PsychErrorExitMsg(PsychError_system, "Failed to create Dictionary for queue.");
		}
		{
			// Get a listing of all elements associated with the device
			// copyMatchinfElements requires IOHIDDeviceInterface122, thus Mac OS X 10.3 or higher
			// elements would have to be obtained directly from IORegistry for 10.2 or earlier
			IOReturn success=(*interface)->copyMatchingElements(interface, NULL, &elements);
			
			if(!elements){
				CFRelease(hidElements);
				(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
				(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
				free(hidDataRef);
				hidDataRef=NULL;
				PsychErrorExitMsg(PsychError_user, "No elements found on device.");
			}
		}
		{
			// Put all appropriate elements into the dictionary and into the queue
			HIDElement newElement;
			CFIndex i;
			for (i=0; i<CFArrayGetCount(elements); i++)
			{
				CFNumberRef number;
				CFDictionaryRef element= CFArrayGetValueAtIndex(elements, i);
				CFTypeRef object;
				
				if(!element) continue;
				bzero(&newElement, sizeof(HIDElement));
				//newElement.owner=hidDataRef;
				
				// Get usage page and make sure it is a keyboard or keypad or something with buttons.
				number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementUsagePageKey));
				if (!number) continue;
				CFNumberGetValue(number, kCFNumberSInt32Type, &newElement.usagePage );
				if((newElement.usagePage != kHIDPage_KeyboardOrKeypad) && (newElement.usagePage != kHIDPage_Button)) continue;

				// If at least one keyboard style device is detected, mark this queue as keyboard queue:
				if (newElement.usagePage == kHIDPage_KeyboardOrKeypad) queueIsAKeyboard = TRUE;

				// Get usage and make sure it is in range 1-256
				number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementUsageKey));
				if (!number) continue;
				CFNumberGetValue(number, kCFNumberSInt32Type, &newElement.usage );
				if(newElement.usage<1 || newElement.usage>256) continue;
				
				// Verify that it is on the queue key list (if specified).
				// Zero value indicates that key corresponding to position should be ignored
				if(psychHIDKbQueueKeyList){
					if(psychHIDKbQueueKeyList[newElement.usage-1]==0) continue;
				}
				
				// Get cookie
				number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementCookieKey));
				if (!number) continue;
				CFNumberGetValue(number, kCFNumberIntType, &(newElement.cookie) );
								
				{
					// Put this element into the hidElements Dictionary
					CFMutableDataRef newData = CFDataCreateMutable(kCFAllocatorDefault, sizeof(HIDElement));
					if (!newData) continue;
					bcopy(&newElement, CFDataGetMutableBytePtr(newData), sizeof(HIDElement));
						  
					number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &newElement.cookie);        
					if (!number) continue;
					CFDictionarySetValue(hidElements, number, newData);
					CFRelease(number);
					CFRelease(newData);
				}
				
				// Put the element cookie into the queue
				result = (*hidDataRef->hidQueueInterface)->addElement(hidDataRef->hidQueueInterface, newElement.cookie, 0);
				if (kIOReturnSuccess != result) continue;
				
				/*
				// Get element type (it should always be selector, but just in case it isn't and this proves pertinent...
				number = (CFNumberRef)CFDictionaryGetValue(element, CFSTR(kIOHIDElementTypeKey));
				if (!number) continue;
				CFNumberGetValue(number, kCFNumberIntType, &(newElement.type) );
				*/
			}
			CFRelease(elements);
		}
		// Make sure that the queue and dictionary aren't empty
		if (CFDictionaryGetCount(hidElements) == 0){
			CFRelease(hidElements);
			(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
			(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
			free(hidDataRef);
			hidDataRef=NULL;
			PsychErrorExitMsg(PsychError_system, "Failed to get any appropriate elements from the device.");
		}
		else{
			hidDataRef->hidElementDictionary = hidElements;
		}
	}

	hidDataRef->eventSource=NULL;
	result = (*hidDataRef->hidQueueInterface)->createAsyncEventSource(hidDataRef->hidQueueInterface, &(hidDataRef->eventSource));
	if (kIOReturnSuccess!=result)
	{
		CFRelease(hidDataRef->hidElementDictionary);
		(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
		(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
		free(hidDataRef);
		hidDataRef=NULL;
		PsychErrorExitMsg(PsychError_system, "Failed to get event source");
	}

	// Allocate memory for tracking keypresses
	psychHIDKbQueueFirstPress=malloc(256*sizeof(AbsoluteTime));
	psychHIDKbQueueFirstRelease=malloc(256*sizeof(AbsoluteTime));
	psychHIDKbQueueLastPress=malloc(256*sizeof(AbsoluteTime));
	psychHIDKbQueueLastRelease=malloc(256*sizeof(AbsoluteTime));
	
	if(!psychHIDKbQueueFirstPress || !psychHIDKbQueueFirstRelease || !psychHIDKbQueueLastPress || !psychHIDKbQueueLastRelease){
		CFRelease(hidDataRef->hidElementDictionary);
		(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
		(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
		free(hidDataRef);
		hidDataRef=NULL;
		PsychErrorExitMsg(PsychError_system, "Failed to allocate memory for tracking keypresses");
	}
	
	// Zero memory for tracking keypresses
	{
		int i;
		for(i=0;i<256;i++){
			psychHIDKbQueueFirstPress[i].hi=0;
			psychHIDKbQueueFirstPress[i].lo=0;
			
			psychHIDKbQueueFirstRelease[i].hi=0;
			psychHIDKbQueueFirstRelease[i].lo=0;
			
			psychHIDKbQueueLastPress[i].hi=0;
			psychHIDKbQueueLastPress[i].lo=0;
			
			psychHIDKbQueueLastRelease[i].hi=0;
			psychHIDKbQueueLastRelease[i].lo=0;
		}
	}

	{
		IOHIDCallbackFunction function=PsychHIDKbQueueCallbackFunction;
		result= (*hidDataRef->hidQueueInterface)->setEventCallout(hidDataRef->hidQueueInterface, function, NULL, hidDataRef);
		if (kIOReturnSuccess!=result)
		{
			free(psychHIDKbQueueFirstPress);
			free(psychHIDKbQueueFirstRelease);
			free(psychHIDKbQueueLastPress);
			free(psychHIDKbQueueLastRelease);
			CFRelease(hidDataRef->hidElementDictionary);
			(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
			(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
			free(hidDataRef);
			hidDataRef=NULL;
			PsychErrorExitMsg(PsychError_system, "Failed to add callout to queue");
		}
	}
	// Initializing an already initialized mutex has undefined results
	// Use pthread_mutex_trylock to determine whether initialization is needed
	{
		int returnCode;
		errno=0;

		pthread_mutex_init(&psychHIDKbQueueMutex, NULL);
		returnCode=pthread_mutex_trylock(&psychHIDKbQueueMutex);
		if(returnCode){
			if(EINVAL==errno){
				// Mutex is invalid, so initialize it
				returnCode=pthread_mutex_init(&psychHIDKbQueueMutex, NULL);
				if(returnCode!=0){
				
					free(psychHIDKbQueueFirstPress);
					free(psychHIDKbQueueFirstRelease);
					free(psychHIDKbQueueLastPress);
					free(psychHIDKbQueueLastRelease);
					CFRelease(hidDataRef->hidElementDictionary);
					(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
					(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
					free(hidDataRef);
					hidDataRef=NULL;
					PsychErrorExitMsg(PsychError_system, "Failed to create mutex");
				}
			}
			else if(EBUSY==errno){
				// Mutex is already locked--not expected, but perhaps recoverable
				errno=0;
				returnCode=pthread_mutex_unlock(&psychHIDKbQueueMutex);
				if(returnCode!=0){
					if(EPERM==errno){
						// Some other thread holds the lock--not recoverable
						free(psychHIDKbQueueFirstPress);
						free(psychHIDKbQueueFirstRelease);
						free(psychHIDKbQueueLastPress);
						free(psychHIDKbQueueLastRelease);
						CFRelease(hidDataRef->hidElementDictionary);
						(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
						(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
						free(hidDataRef);
						hidDataRef=NULL;
						PsychErrorExitMsg(PsychError_system, "Another thread holds the lock on the mutex");
					}
				}
			}
		}
		else{
			// Attempt to lock was successful, so unlock
			pthread_mutex_unlock(&psychHIDKbQueueMutex);
		}
	}

	// Create event buffer:
	PsychHIDCreateEventBuffer(0);

	{
		int returnCode=pthread_create(&psychHIDKbQueueThread, NULL, PsychHIDKbQueueNewThread, NULL);
		if(returnCode!=0){
			free(psychHIDKbQueueFirstPress);
			free(psychHIDKbQueueFirstRelease);
			free(psychHIDKbQueueLastPress);
			free(psychHIDKbQueueLastRelease);
			CFRelease(hidDataRef->hidElementDictionary);
			(*hidDataRef->hidQueueInterface)->dispose(hidDataRef->hidQueueInterface);
			(*hidDataRef->hidQueueInterface)->Release(hidDataRef->hidQueueInterface);
			free(hidDataRef);
			hidDataRef=NULL;
			PsychErrorExitMsg(PsychError_system, "Failed to create thread");
		}
	}

	return(PsychError_none);	
}

int PsychHIDGetDefaultKbQueueDevice(void)
{
	return(0);
}

#endif
