/*
	PsychToolbox3/Source/Linux/Screen/PsychScreenGlue.h
	
	PLATFORMS:	
	
		This is the Linux/X11 version only.  
				
	AUTHORS:
	
	Mario Kleiner		mk		mario.kleiner at tuebingen.mpg.de

	HISTORY:
	
	2/20/06                 mk              Wrote it. Derived from Windows version.
        							
	DESCRIPTION:
	
		Functions in this file comprise an abstraction layer for probing and controlling screen state.  
		
		Each C function which implements a particular Screen subcommand should be platform neutral.  For example, the source to SCREENPixelSizes() 
		should be platform-neutral, despite that the calls in OS X and Linux to detect available pixel sizes are
		different.  The platform specificity is abstracted out in C files which end it "Glue", for example PsychScreenGlue, PsychWindowGlue, 
		PsychWindowTextClue.
	
		In addition to glue functions for windows and screen there are functions which implement shared functionality between between Screen commands,
		such as ScreenTypes.c and WindowBank.c. 
			
	NOTES:
	
	TO DO: 

*/

//include once
#ifndef PSYCH_IS_INCLUDED_PsychScreenGlue
#define PSYCH_IS_INCLUDED_PsychScreenGlue

#include "Screen.h"

//functions from PsychScreenGlue
void						InitializePsychDisplayGlue(void);
void						PsychGetCGDisplayIDFromScreenNumber(CGDirectDisplayID *displayID, int screenNumber);
void						PsychCaptureScreen(int screenNumber);
void						PsychReleaseScreen(int screenNumber);
psych_bool						PsychIsScreenCaptured(int screenNumber);
int						PsychGetNumDisplays(void);
void						PsychGetScreenDepths(int screenNumber, PsychDepthType *depths);
int						PsychGetAllSupportedScreenSettings(int screenNumber, long** widths, long** heights, long** hz, long** bpp);
psych_bool						PsychCheckVideoSettings(PsychScreenSettingsType *setting);
void						PsychGetScreenDepth(int screenNumber, PsychDepthType *depth);   //dont' use this and get rid  of it.
int						PsychGetScreenDepthValue(int screenNumber);
int						PsychGetNumScreenPlanes(int screenNumber);
float						PsychGetNominalFramerate(int screenNumber);
float                       			PsychSetNominalFramerate(int screenNumber, float requestedHz);
void						PsychGetScreenSize(int screenNumber, long *width, long *height);
void						PsychGetGlobalScreenRect(int screenNumber, double *rect);
void						PsychGetScreenRect(int screenNumber, double *rect);
void                        			PsychGetDisplaySize(int screenNumber, int *width, int *height);
PsychColorModeType				PsychGetScreenMode(int screenNumber);
int						PsychGetDacBitsFromDisplay(int screenNumber);		//from display, not from preferences
void						PsychGetScreenSettings(int screenNumber, PsychScreenSettingsType *settings);
psych_bool						PsychSetScreenSettings(psych_bool cacheSettings, PsychScreenSettingsType *settings);
psych_bool						PsychRestoreScreenSettings(int screenNumber);
void						PsychHideCursor(int screenNumber);
void						PsychShowCursor(int screenNumber);
void						PsychPositionCursor(int screenNumber, int x, int y);
void						PsychReadNormalizedGammaTable(int screenNumber, int *numEntries, float **redTable, float **greenTable, float **blueTable);
void						PsychLoadNormalizedGammaTable(int screenNumber, int numEntries, float *redTable, float *greenTable, float *blueTable);
int                         			PsychGetDisplayBeamPosition(CGDirectDisplayID cgDisplayId, int screenNumber);
PsychError					PsychOSSynchronizeDisplayScreens(int *numScreens, int* screenIds, int* residuals, unsigned int syncMethod, double syncTimeOut, int allowedResidual);
void						PsychOSShutdownPsychtoolboxKernelDriverInterface(void);
unsigned int					PsychOSKDReadRegister(int screenId, unsigned int offset, unsigned int* status);
unsigned int					PsychOSKDWriteRegister(int screenId, unsigned int offset, unsigned int value, unsigned int* status);
psych_bool						PsychOSIsKernelDriverAvailable(int screenId);

// Internal helper routines for memory mapped gfx-hardware register low level access: Called
// from PsychWindowGlue.c PsychOSOpenOnscreenWindow() and PsychOSCloseOnscreenWindow() routines
// to setup and tear-down memory mappings...
psych_bool 					PsychScreenMapRadeonCntlMemory(void);
void 						PsychScreenUnmapDeviceMemory(void);

//end include once
#endif

