/*
	PsychToolbox3/Source/Linux/Screen/PsychWindowGlue.h
	
	PLATFORMS:	
	
		This is the Linux/X11 version only.  
				
	AUTHORS:
	
		Allen Ingling		awi		Allen.Ingling@nyu.edu
                Mario Kleiner           mk              mario.kleiner at tuebingen.mpg.de

	HISTORY:
	
	        2/20/06                 mk              Created - Derived from Windows version.

	DESCRIPTION:
	
		Functions in this file comprise an abstraction layer for probing and controlling window state, except for window content.  
		
		Each C function which implements a particular Screen subcommand should be platform neutral.  For example, the source to SCREENPixelSizes() 
		should be platform-neutral, despite that the calls in OS X and Linux to detect available pixel sizes are different.  The platform 
		specificity is abstracted out in C files which end it "Glue", for example PsychScreenGlue, PsychWindowGlue, PsychWindowTextClue.

	NOTES:
	
	TO DO: 
	 
*/

//include once
#ifndef PSYCH_IS_INCLUDED_PsychWindowGlue
#define PSYCH_IS_INCLUDED_PsychWindowGlue

#include "Screen.h"

boolean PsychRealtimePriority(boolean enable_realtime);
boolean PsychOSOpenOnscreenWindow(PsychScreenSettingsType *screenSettings, PsychWindowRecordType *windowRecord, int numBuffers, int stereomode, int conserveVRAM);
boolean PsychOSOpenOffscreenWindow(double *rect, int depth, PsychWindowRecordType **windowRecord);
void	PsychOSCloseOnscreenWindow(PsychWindowRecordType *windowRecord);
void	PsychOSCloseWindow(PsychWindowRecordType *windowRecord);
void	PsychOSCloseOffscreenWindow(PsychWindowRecordType *windowRecord);
void    PsychOSFlipWindowBuffers(PsychWindowRecordType *windowRecord);
void    PsychOSSetVBLSyncLevel(PsychWindowRecordType *windowRecord, int swapInterval);
void	PsychOSSetGLContext(PsychWindowRecordType *windowRecord);
void	PsychOSUnsetGLContext(PsychWindowRecordType *windowRecord);
void    PsychOSSetUserGLContext(PsychWindowRecordType *windowRecord, Boolean copyfromPTBContext);

// Special query function, only defined on Linux/X11:
int PsychGetXScreenIdForScreen(int screenNumber);

//end include once
#endif



