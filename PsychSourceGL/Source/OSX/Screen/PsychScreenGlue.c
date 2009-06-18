/*
	PsychToolbox3/Source/OSX/Screen/PsychScreenGlue.c
	
	PLATFORMS:	
	
		This is the OS X Core Graphics version only.  
				
	AUTHORS:
	
		Allen Ingling		awi		Allen.Ingling@nyu.edu

	HISTORY:
	
		10/20/02		awi			Wrote it mostly by adding in SDL-specific refugeess (from an experimental SDL Psychtoolbox) from header and source files.
		11/16/04		awi			added  PsychGetGlobalScreenRect.  Enhanced DESCRIPTION text.  
		04/29/05        mk          Return id of primary display for displays in hardware mirroring sets.
		12/29/06		mk			Implement query code for DAC output resolution on OS-X, finally...
        							
	DESCRIPTION:
	
		Functions in this file comprise an abstraction layer for probing and controlling screen state.  
		
		Each C function which implements a particular Screen subcommand should be platform neutral.  For example, the source to SCREENPixelSizes() 
		should be platform-neutral, despite that the calls in OS X and Windows to detect available pixel sizes are
		different.  The platform specificity is abstracted out in C files which end it "Glue", for example PsychScreenGlue, PsychWindowGlue, 
		PsychWindowTextClue.
	
		In addition to glue functions for windows and screen there are functions which implement shared functionality between between Screen commands,
		such as ScreenTypes.c and WindowBank.c. 
			
	NOTES:
	
	TO DO: 
	
		� The "glue" files should should be suffixed with a platform name.  The original (bad) plan was to distingish platform-specific files with the same 
		name by their placement in a directory tree.
		
		� All of the functions which accept a screen number should be suffixed with "...FromScreenNumber". 
*/


#include "Screen.h"

// Include IOKIT support for connecting to the kernel level support driver:
#include <IOKit/IOKitLib.h>
// Include shared data structures and definitions between kernel driver and us:
#include "PsychUserKernelShared.h"
// Include specifications of the GPU registers:
#include "PsychGraphicsCardRegisterSpecs.h"

#define kMyPathToSystemLog			"/var/log/system.log"

// file local variables

// Maybe use NULLs in the settings arrays to mark entries invalid instead of using psych_bool flags in a different array.   
static psych_bool				displayLockSettingsFlags[kPsychMaxPossibleDisplays];
static CFDictionaryRef		displayOriginalCGSettings[kPsychMaxPossibleDisplays];        	//these track the original video state before the Psychtoolbox changed it.  
static psych_bool				displayOriginalCGSettingsValid[kPsychMaxPossibleDisplays];
static CFDictionaryRef		displayOverlayedCGSettings[kPsychMaxPossibleDisplays];        	//these track settings overlayed with 'Resolutions'.  
static psych_bool				displayOverlayedCGSettingsValid[kPsychMaxPossibleDisplays];
static CGDisplayCount 		numDisplays, numPhysicalDisplays;
static CGDirectDisplayID 	displayCGIDs[kPsychMaxPossibleDisplays];
static CGDirectDisplayID 	displayOnlineCGIDs[kPsychMaxPossibleDisplays];

// List of service connect handles for connecting to the kernel support driver (if any):
static int					numKernelDrivers = 0;
static io_connect_t			displayConnectHandles[kPsychMaxPossibleDisplays];
static int					repeatedZeroBeamcount[kPsychMaxPossibleDisplays];

//file local functions
void InitCGDisplayIDList(void);
void PsychLockScreenSettings(int screenNumber);
void PsychUnlockScreenSettings(int screenNumber);
psych_bool PsychCheckScreenSettingsLock(int screenNumber);
psych_bool PsychGetCGModeFromVideoSetting(CFDictionaryRef *cgMode, PsychScreenSettingsType *setting);
void InitPsychtoolboxKernelDriverInterface(void);
kern_return_t PsychOSKDDispatchCommand(io_connect_t connect, const PsychKDCommandStruct* inStruct, PsychKDCommandStruct* outStruct, unsigned int* status);
io_connect_t PsychOSCheckKDAvailable(int screenId, unsigned int * status);
int PsychOSKDGetBeamposition(int screenId);
void PsychLaunchConsoleApp(void);

//Initialization functions
void InitializePsychDisplayGlue(void)
{
    int i;
    
    //init the display settings flags.
    for(i=0;i<kPsychMaxPossibleDisplays;i++){
        displayLockSettingsFlags[i]=FALSE;
        displayOriginalCGSettingsValid[i]=FALSE;
        displayOverlayedCGSettingsValid[i]=FALSE;
		displayConnectHandles[i]=0;
		repeatedZeroBeamcount[i]=0;
    }
    
    // Init the list of Core Graphics display IDs.
    InitCGDisplayIDList();
	
	// Attach to kernel-level Psychtoolbox graphics card interface driver if possible
	// *and* allowed by settings, setup all relevant mappings:
	InitPsychtoolboxKernelDriverInterface();
}

// MK-TODO: There's still a bug here: We need to call InitCGDisplayIDList() not only at
// Screen-Init time but also as part of *EACH* query to the list...
// Otherwise, if the user changes display settings (layout of displays, primary<->secondary display,
// mirror<->non-mirror mode) or connects/disconnects/replugs/powers on or powers off displays,
// while a Matlab session is running and without "clear Screen"
// after the change, PTB will not notice the change in display configuration and access
// invalid or wrong display handles --> all kind of syncing problems and weird bugs...
void InitCGDisplayIDList(void)
{
    CGDisplayErr error;
    // MK: This kills syncing in mirrored-setups: error = CGGetActiveDisplayList(kPsychMaxPossibleDisplays, displayCGIDs, &numDisplays);
    // Therefore we query the list of displays that are "online" - That means: Connected and powered on.
    // Displays that are in power-saving mode (asleep) or not in the ActiveDisplayList,
    // because they are part of a mirror set and therefore not drawable, are still in the online list.
    // We need handles for displays that are in a mirror set in order to send our beampos-queries
    // and CGLFlushDrawable - requests to them -- Otherwise time-stamping and sync of bufferswap
    // to VBL can fail due to syncing to / querying the wrong display in a mirror set...
    // Currently we accept system failure in case of user switching on/off displays during a session...
    // error = CGGetOnlineDisplayList(kPsychMaxPossibleDisplays, displayCGIDs, &numDisplays);
    error = CGGetActiveDisplayList(kPsychMaxPossibleDisplays, displayCGIDs, &numDisplays);
    if(error) PsychErrorExitMsg(PsychError_internal, "CGGetActiveDisplayList failed to enumerate displays");
    
	// Also enumerate physical displays:
    error = CGGetOnlineDisplayList(kPsychMaxPossibleDisplays, displayOnlineCGIDs, &numPhysicalDisplays);
    if(error) PsychErrorExitMsg(PsychError_internal, "CGGetOnlineDisplayList failed to enumerate displays");

    // TESTCODE:
    if (false) {
        unsigned int testvals[kPsychMaxPossibleDisplays*100];    
        int i;
        for (i=0; i<numDisplays*100; i++) {
            testvals[i]=0;
        }
        for (i=0; i<numDisplays*100; i++) {
            testvals[i]=CGDisplayBeamPosition(displayCGIDs[i % numDisplays]);
        }
        
        for (i=0; i<numDisplays*100; i++) {
            mexPrintf("PTB TESTCODE : Display %i : beampos %i\n", i % numDisplays, testvals[i]);
        }        
    }
}


void PsychGetCGDisplayIDFromScreenNumber(CGDirectDisplayID *displayID, int screenNumber)
{
    if(screenNumber>= (int) numDisplays) PsychErrorExit(PsychError_invalidScumber);
	
	if (screenNumber < 0) {
		// Special case: Physical displays handle: Put back into positive range and
		// correct for 1-based external indexing:
		screenNumber = (-1 * screenNumber) - 1;
		if (screenNumber >= (int) numPhysicalDisplays) PsychErrorExitMsg(PsychError_user, "Invalid physical screenNumber provided! Higher than number of connected physical displays!");
		
		// Valid range: Map it:
		*displayID=displayOnlineCGIDs[screenNumber];

		return;
	}
	
    // Standard case: Logical displays:
	
	// MK: We return the id of the primary display of the hardware-mirror set to which
    // the display for 'screenNumber' belongs to. This will be the same display on
    // single display setups. On dual-display setups, it will return the ID of the
    // display we are really syncing in Screen('Flip'). This is important for querying
    // the rasterbeam position of the correct display in such setups.
    //
    // I'm not sure if this is the best place for performing this lookup, but
    // at least it should be safe to do here...
    *displayID=CGDisplayPrimaryDisplay(displayCGIDs[screenNumber]);

    // Protection against Apples stupidity... - our our's if we made false assumptions ;-)
    if (CGDisplayUnitNumber(*displayID)!=CGDisplayUnitNumber(displayCGIDs[screenNumber])) {
        mexPrintf("PTB-DEBUG : ACTIVE DISPLAY <-> PRIMARY DISPLAY MISMATCH FOR SCREEN %i!!!!\n", screenNumber);
    }
	
	return;
}


/*  About locking display settings:

    SCREENOpenWindow and SCREENOpenOffscreenWindow  set the lock when opening  windows and 
    SCREENCloseWindow unsets upon the close of the last of a screen's windows. PsychSetVideoSettings checks for a lock
    before changing the settings.  Anything (SCREENOpenWindow or SCREENResolutions) which attemps to change
    the display settings should report that attempts to change a dipslay's settings are not allowed when its windows are open.
    
    PsychSetVideoSettings() may be called by either SCREENOpenWindow or by Resolutions().  If called by Resolutions it both sets the video settings
    and caches the video settings so that subsequent calls to OpenWindow can use the cached mode regardless of whether interceding calls to OpenWindow
    have changed the display settings or reverted to the virgin display settings by closing.  SCREENOpenWindow should thus invoke the cached mode
    settings if they have been specified and not current actual display settings.  
    
*/    
    


void PsychLockScreenSettings(int screenNumber)
{
    displayLockSettingsFlags[screenNumber]=TRUE;
}

void PsychUnlockScreenSettings(int screenNumber)
{
    displayLockSettingsFlags[screenNumber]=FALSE;
}

psych_bool PsychCheckScreenSettingsLock(int screenNumber)
{
    return(displayLockSettingsFlags[screenNumber]);
}


/* Because capture and lock will always be used in conjuction, capture calls lock, and SCREENOpenWindow must only call Capture and Release */
void PsychCaptureScreen(int screenNumber)
{
    CGDisplayErr  error;
    
    if(screenNumber>=numDisplays)
        PsychErrorExit(PsychError_invalidScumber);
    error=CGDisplayCapture(displayCGIDs[screenNumber]);
    if(error)
        PsychErrorExitMsg(PsychError_internal, "Unable to capture display");
    PsychLockScreenSettings(screenNumber);
}

/*
    PsychReleaseScreen()
    
*/
void PsychReleaseScreen(int screenNumber)
{	
    CGDisplayErr  error;
    
    if(screenNumber>=numDisplays)
        PsychErrorExit(PsychError_invalidScumber);
    error=CGDisplayRelease(displayCGIDs[screenNumber]);
    if(error)
        PsychErrorExitMsg(PsychError_internal, "Unable to release display");
    PsychUnlockScreenSettings(screenNumber);
}

psych_bool PsychIsScreenCaptured(screenNumber)
{
    return(PsychCheckScreenSettingsLock(screenNumber));
}    


//Read display parameters.
/*
    PsychGetNumDisplays()
    Get the number of video displays connected to the system.
*/
int PsychGetNumDisplays(void)
{
    return((int)numDisplays);
}

/* This is only defined on OS/X for now: */
int PsychGetNumPhysicalDisplays(void)
{
    return((int) numPhysicalDisplays);
}

void PsychGetScreenDepths(int screenNumber, PsychDepthType *depths)
{
    CFDictionaryRef currentMode, tempMode;
    CFArrayRef modeList;
    CFNumberRef n;
    int i, numPossibleModes;
    long currentWidth, currentHeight, tempWidth, tempHeight, currentFrequency, tempFrequency, tempDepth;

    if(screenNumber>=numDisplays)
        PsychErrorExit(PsychError_invalidScumber); //also checked within SCREENPixelSizes
     
    //Get the current display mode.  We will want to match against width and hz when looking for available depths. 
    currentMode = CGDisplayCurrentMode(displayCGIDs[screenNumber]);
    n=CFDictionaryGetValue(currentMode, kCGDisplayWidth);
    CFNumberGetValue(n,kCFNumberLongType, &currentWidth);
    n=CFDictionaryGetValue(currentMode, kCGDisplayHeight);
    CFNumberGetValue(n,kCFNumberLongType, &currentHeight);
    n=CFDictionaryGetValue(currentMode, kCGDisplayRefreshRate );
    CFNumberGetValue(n, kCFNumberLongType, &currentFrequency ) ;

    //get a list of avialable modes for the specified display
    modeList = CGDisplayAvailableModes(displayCGIDs[screenNumber]);
    numPossibleModes= CFArrayGetCount(modeList);
    for(i=0;i<numPossibleModes;i++){
        tempMode = CFArrayGetValueAtIndex(modeList,i);
        n=CFDictionaryGetValue(tempMode, kCGDisplayWidth);
        CFNumberGetValue(n,kCFNumberLongType, &tempWidth);
        n=CFDictionaryGetValue(tempMode, kCGDisplayHeight);
        CFNumberGetValue(n,kCFNumberLongType, &tempHeight);
        n=CFDictionaryGetValue(tempMode, kCGDisplayRefreshRate);
        CFNumberGetValue(n, kCFNumberLongType, &tempFrequency) ;
        if(currentWidth==tempWidth && currentHeight==tempHeight && currentFrequency==tempFrequency){
            n=CFDictionaryGetValue(tempMode, kCGDisplayBitsPerPixel);
            CFNumberGetValue(n, kCFNumberLongType, &tempDepth) ;
            PsychAddValueToDepthStruct((int)tempDepth, depths);
        }
		// printf("mode %i : w x h = %i x %i, fps = %i, depths = %i\n", i, tempWidth, tempHeight, tempFrequency, tempDepth);
    }

}

/*   PsychGetAllSupportedScreenSettings()
 *
 *	 Queries the display system for a list of all supported display modes, ie. all valid combinations
 *	 of resolution, pixeldepth and refresh rate. Allocates temporary arrays for storage of this list
 *	 and returns it to the calling routine. This function is basically only used by Screen('Resolutions').
 */
int PsychGetAllSupportedScreenSettings(int screenNumber, long** widths, long** heights, long** hz, long** bpp)
{
    CFDictionaryRef tempMode;
    CFArrayRef modeList;
    CFNumberRef n;
    int i, numPossibleModes;
    long tempWidth, tempHeight, currentFrequency, tempFrequency, tempDepth;

    if(screenNumber>=numDisplays) PsychErrorExit(PsychError_invalidScumber);

    // Get a list of avialable modes for the specified display:
    modeList = CGDisplayAvailableModes(displayCGIDs[screenNumber]);
    numPossibleModes= CFArrayGetCount(modeList);
	
	// Allocate output arrays: These will get auto-released at exit
	// from Screen():
	*widths = (long*) PsychMallocTemp(numPossibleModes * sizeof(int));
	*heights = (long*) PsychMallocTemp(numPossibleModes * sizeof(int));
	*hz = (long*) PsychMallocTemp(numPossibleModes * sizeof(int));
	*bpp = (long*) PsychMallocTemp(numPossibleModes * sizeof(int));
	
	// Fetch modes and store into arrays:
    for(i=0; i<numPossibleModes; i++) {
        tempMode = CFArrayGetValueAtIndex(modeList,i);
        n=CFDictionaryGetValue(tempMode, kCGDisplayWidth);
        CFNumberGetValue(n,kCFNumberLongType, &tempWidth);
		(*widths)[i] = tempWidth;
		
        n=CFDictionaryGetValue(tempMode, kCGDisplayHeight);
        CFNumberGetValue(n,kCFNumberLongType, &tempHeight);
		(*heights)[i] = tempHeight;

        n=CFDictionaryGetValue(tempMode, kCGDisplayRefreshRate);
        CFNumberGetValue(n, kCFNumberLongType, &tempFrequency) ;
		(*hz)[i] = tempFrequency;

		n=CFDictionaryGetValue(tempMode, kCGDisplayBitsPerPixel);
		CFNumberGetValue(n, kCFNumberLongType, &tempDepth) ;
		(*bpp)[i] = tempDepth;
    }

	return(numPossibleModes);
}

/*
    static PsychGetCGModeFromVideoSettings()
   
*/
psych_bool PsychGetCGModeFromVideoSetting(CFDictionaryRef *cgMode, PsychScreenSettingsType *setting)
{
    CFArrayRef modeList;
    CFNumberRef n;
    int i, numPossibleModes;
    long width, height, depth, frameRate, tempWidth, tempHeight, tempDepth,  tempFrameRate;
    
    if(setting->screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetScreenDepths() is out of range"); //also checked within SCREENPixelSizes
        
    //adjust parameter formats
    width=(long)PsychGetWidthFromRect(setting->rect);
    height=(long)PsychGetHeightFromRect(setting->rect);
    depth=(long)PsychGetValueFromDepthStruct(0,&(setting->depth));
    frameRate=(long)setting->nominalFrameRate;

    //get a list of avialable modes for the specified display and iterate over the list looking for our mode.
    modeList = CGDisplayAvailableModes(displayCGIDs[setting->screenNumber]);
    numPossibleModes= CFArrayGetCount(modeList);
    for(i=0;i<numPossibleModes;i++){
        *cgMode = CFArrayGetValueAtIndex(modeList,i);			
        n=CFDictionaryGetValue(*cgMode, kCGDisplayWidth);		//width
        CFNumberGetValue(n,kCFNumberLongType, &tempWidth);
        n=CFDictionaryGetValue(*cgMode, kCGDisplayHeight);		//height
        CFNumberGetValue(n,kCFNumberLongType, &tempHeight);
        n=CFDictionaryGetValue(*cgMode, kCGDisplayRefreshRate);	//frequency
        CFNumberGetValue(n, kCFNumberLongType, &tempFrameRate) ;
        n=CFDictionaryGetValue(*cgMode, kCGDisplayBitsPerPixel);	//depth
        CFNumberGetValue(n, kCFNumberLongType, &tempDepth) ;
        if(width==tempWidth && height==tempHeight && frameRate==tempFrameRate && depth==tempDepth)
            return(TRUE);
    }
    return(FALSE);    
}


/*
    PsychCheckVideoSettings()
    
    Check all available video display modes for the specified screen number and return true if the 
    settings are valid and false otherwise.
*/
psych_bool PsychCheckVideoSettings(PsychScreenSettingsType *setting)
{
        CFDictionaryRef cgMode;
        
        return(PsychGetCGModeFromVideoSetting(&cgMode, setting));
}



/*
    PsychGetScreenDepth()
    
    The caller must allocate and initialize the depth struct. 
*/
void PsychGetScreenDepth(int screenNumber, PsychDepthType *depth)
{
    
    if(screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber is out of range"); //also checked within SCREENPixelSizes
    PsychAddValueToDepthStruct((int)CGDisplayBitsPerPixel(displayCGIDs[screenNumber]),depth);

}

int PsychGetScreenDepthValue(int screenNumber)
{
    PsychDepthType	depthStruct;
    
    PsychInitDepthStruct(&depthStruct);
    PsychGetScreenDepth(screenNumber, &depthStruct);
    return(PsychGetValueFromDepthStruct(0,&depthStruct));
}


float PsychGetNominalFramerate(int screenNumber)
{
    CFDictionaryRef currentMode;
    CFNumberRef n;
    double currentFrequency;
    
    //Get the CG display ID index for the specified display
    if(screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber is out of range"); 
    currentMode = CGDisplayCurrentMode(displayCGIDs[screenNumber]);
    n=CFDictionaryGetValue(currentMode, kCGDisplayRefreshRate);
    CFNumberGetValue(n, kCFNumberDoubleType, &currentFrequency);
    return(currentFrequency);
}

void PsychGetScreenSize(int screenNumber, long *width, long *height)
{
    CFDictionaryRef currentMode;
    CFNumberRef n;
    
    if(screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetScreenDepths() is out of range"); 
    currentMode = CGDisplayCurrentMode(displayCGIDs[screenNumber]);
    n=CFDictionaryGetValue(currentMode, kCGDisplayWidth);
    CFNumberGetValue(n,kCFNumberLongType, width); 
    n=CFDictionaryGetValue(currentMode, kCGDisplayHeight);
    CFNumberGetValue(n,kCFNumberLongType, height);

}

/* Returns the physical display size as reported by OS-X: */
void PsychGetDisplaySize(int screenNumber, int *width, int *height)
{
    CGSize physSize;
    if(screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetDisplaySize() is out of range");
    physSize = CGDisplayScreenSize(displayCGIDs[screenNumber]);
    *width = (int) physSize.width;
    *height = (int) physSize.height;
}

void PsychGetGlobalScreenRect(int screenNumber, double *rect)
{
	CGDirectDisplayID	displayID;
	CGRect				cgRect;
	double				rLeft, rRight, rTop, rBottom;

    if(screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetScreenDepths() is out of range"); 
	displayID=displayCGIDs[screenNumber];
	cgRect=CGDisplayBounds(displayID);
	rLeft=cgRect.origin.x;
	rTop=cgRect.origin.y;
	rRight=cgRect.origin.x + cgRect.size.width;
	rBottom=cgRect.origin.y + cgRect.size.height;
	PsychMakeRect(rect, rLeft, rTop, rRight, rBottom);
	
}


void PsychGetScreenRect(int screenNumber, double *rect)
{
    long width, height; 

    PsychGetScreenSize(screenNumber, &width, &height);
    rect[kPsychLeft]=0;
    rect[kPsychTop]=0;
    rect[kPsychRight]=(int)width;
    rect[kPsychBottom]=(int)height; 
} 


PsychColorModeType PsychGetScreenMode(int screenNumber)
{
    PsychDepthType depth;
        
    PsychInitDepthStruct(&depth);
    PsychGetScreenDepth(screenNumber, &depth);
    return(PsychGetColorModeFromDepthStruct(&depth));
}


/*
    Its probably better to read this directly from the CG renderer info than to infer it from the pixel size
*/	
int PsychGetNumScreenPlanes(int screenNumber)
{
    return((PsychGetScreenDepthValue(screenNumber)>24) ? 4 : 3 );
}



/*
	PsychGetDacBitsFromDisplay()
	
	Return output resolution of video DAC in bits per color component.
	We return a safe default of 8 bpc if we can't query the real value.
*/
int PsychGetDacBitsFromDisplay(int screenNumber)
{
    CGDirectDisplayID	displayID;
	CFMutableDictionaryRef properties;
	CFNumberRef cfGammaWidth;
	SInt32 dacbits;
	io_service_t displayService;
	kern_return_t kr;

	// Retrieve display handle for screen:
	PsychGetCGDisplayIDFromScreenNumber(&displayID, screenNumber);

	// Retrieve low-level IOKit service port for this display:
	displayService = CGDisplayIOServicePort(displayID);
	// printf("Display 0x%08X with IOServicePort 0x%08X\n", displayID, displayService);
	
	// Obtain the properties from that service
	kr = IORegistryEntryCreateCFProperties(displayService, &properties, NULL, 0);
	if((kr == kIOReturnSuccess) && ((cfGammaWidth = (CFNumberRef) CFDictionaryGetValue(properties, CFSTR(kIOFBGammaWidthKey)))!=NULL))
	{
		CFNumberGetValue(cfGammaWidth, kCFNumberSInt32Type, &dacbits);
		CFRelease(properties);
		return((int) dacbits);
	}
	else {
		// Failed! Return safe 8 bits...
		CFRelease(properties);
		if (PsychPrefStateGet_Verbosity()>1) printf("PTB-WARNING: Failed to query resolution of video DAC for screen %i! Will return safe default of 8 bits.\n", screenNumber);
		return(8);
	}
}



/*
    PsychGetVideoSettings()
    
    Fills a structure describing the screen settings such as x, y, depth, frequency, etc.
    
    Consider inverting the calling sequence so that this function is at the bottom of call hierarchy.  
*/ 
void PsychGetScreenSettings(int screenNumber, PsychScreenSettingsType *settings)
{
    settings->screenNumber=screenNumber;
    PsychGetScreenRect(screenNumber, settings->rect);
    PsychInitDepthStruct(&(settings->depth));
    PsychGetScreenDepth(screenNumber, &(settings->depth));
    settings->mode=PsychGetColorModeFromDepthStruct(&(settings->depth));
    settings->nominalFrameRate=PsychGetNominalFramerate(screenNumber);
	// settings->dacbits=PsychGetDacBitsFromDisplay(screenNumber);
}

//Set display parameters

/*
    PsychSetScreenSettings()
	
	Accept a PsychScreenSettingsType structure holding a video mode and set the display mode accordingly.
    
    If we can not change the display settings because of a lock (set by open window or close window) then return false.
    
    SCREENOpenWindow should capture the display before it sets the video mode.  If it doesn't, then PsychSetVideoSettings will
    detect that and exit with an error.  SCREENClose should uncapture the display. 
    
    The duties of SCREENOpenWindow are:
    -Lock the screen which serves the purpose of preventing changes in video setting with open Windows.
    -Capture the display which gives the application synchronous control of display parameters.
    
    TO DO: for 8-bit palletized mode there is probably more work to do.  
      
*/

psych_bool PsychSetScreenSettings(psych_bool cacheSettings, PsychScreenSettingsType *settings)
{
    CFDictionaryRef 		cgMode;
    psych_bool 			isValid, isCaptured;
    CGDisplayErr 		error;

    //get the display IDs.  Maybe we should consolidate this out of these functions and cache the IDs in a file static
    //variable, since basicially every core graphics function goes through this deal.    
    if(settings->screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetScreenDepths() is out of range"); //also checked within SCREENPixelSizes

    //Check for a lock which means onscreen or offscreen windows tied to this screen are currently open.
    // MK: Disabled if(PsychCheckScreenSettingsLock(settings->screenNumber)) return(false);  //calling function should issue an error for attempt to change display settings while windows were open.
    
    
    //store the original display mode if this is the first time we have called this function.  The psychtoolbox will disregard changes in 
    //the screen state made through the control panel after the Psychtoolbox was launched. That is, OpenWindow will by default continue to 
    //open windows with finder settings which were in place at the first call of OpenWindow.  That's not intuitive, but not much of a problem
    //either. 
    if(!displayOriginalCGSettingsValid[settings->screenNumber]){
        displayOriginalCGSettings[settings->screenNumber]=CGDisplayCurrentMode(displayCGIDs[settings->screenNumber]);
        displayOriginalCGSettingsValid[settings->screenNumber]=TRUE;
    }
    
    //Find core graphics video settings which correspond to settings as specified withing by an abstracted psychsettings structure.  
    isValid=PsychGetCGModeFromVideoSetting(&cgMode, settings);
    if(!isValid){
        PsychErrorExitMsg(PsychError_internal, "Attempt to set invalid video settings"); 
        //this is an internal error because the caller is expected to check first. 
    }
    
    //If the caller passed cache settings (then it is SCREENResolutions) and we should cache the current video mode settings for this display.  These
    //are cached in the form of CoreGraphics settings and not Psychtoolbox video settings.  The only caller which should pass a set cache flag is 
    //SCREENResolutions
    if(cacheSettings){
        displayOverlayedCGSettings[settings->screenNumber]=cgMode;
        displayOverlayedCGSettingsValid[settings->screenNumber]=TRUE;
    }
    
    //Check to make sure that this display is captured, which OpenWindow should have done.  If it has not been done, then exit with an error.  
    isCaptured=CGDisplayIsCaptured(displayCGIDs[settings->screenNumber]);
    if(!isCaptured)
        PsychErrorExitMsg(PsychError_internal, "Attempt to change video settings without capturing the display");
        
    //Change the display mode.   
    error=CGDisplaySwitchToMode(displayCGIDs[settings->screenNumber], cgMode);
    
    return(error == (int) 0);
}

/*
    PsychRestoreVideoSettings()
    
    Restores video settings to the state set by the finder.  Returns TRUE if the settings can be restored or false if they 
    can not be restored because a lock is in effect, which would mean that there are still open windows.    
    
*/
psych_bool PsychRestoreScreenSettings(int screenNumber)
{
    psych_bool 			isCaptured;
    CGDisplayErr 		error;

    if(screenNumber>=numDisplays) PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychRestoreScreenSettings() is out of range");

    //Check for a lock which means onscreen or offscreen windows tied to this screen are currently open.
    // MK: Disabled    if(PsychCheckScreenSettingsLock(screenNumber)) return(false);  //calling function will issue error for attempt to change display settings while windows were open.
    
    //Check to make sure that the original graphics settings were cached.  If not, it means that the settings were never changed, so we can just
    //return true. 
    if(!displayOriginalCGSettingsValid[screenNumber]) return(true);
    
    //Check to make sure that this display is captured, which OpenWindow should have done.  If it has not been done, then exit with an error.  
    isCaptured=CGDisplayIsCaptured(displayCGIDs[screenNumber]);
    if(!isCaptured) PsychErrorExitMsg(PsychError_internal, "Attempt to change video settings without capturing the display");
    
    //Change the display mode.   
    error=CGDisplaySwitchToMode(displayCGIDs[screenNumber], displayOriginalCGSettings[screenNumber]);
    if(error) PsychErrorExitMsg(PsychError_internal, "Unable to set switch video modes");

    return(true);
}

void PsychHideCursor(int screenNumber)
{

    CGDisplayErr 	error;
    CGDirectDisplayID	cgDisplayID;
    
    PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, screenNumber);
    error=CGDisplayHideCursor(cgDisplayID);
    if(error)
        PsychErrorExit(PsychError_internal);

}

void PsychShowCursor(int screenNumber)
{

    CGDisplayErr 	error;
    CGDirectDisplayID	cgDisplayID;
    
    PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, screenNumber);
    error=CGDisplayShowCursor(cgDisplayID);
    if(error)
        PsychErrorExit(PsychError_internal);

}

void PsychPositionCursor(int screenNumber, int x, int y)
{
    CGDisplayErr 	error;
    CGDirectDisplayID	cgDisplayID;
    CGPoint 		point;
    
    PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, screenNumber);
    point.x=(float)x;
    point.y=(float)y;
    error=CGDisplayMoveCursorToPoint(cgDisplayID, point); 
    if(error)
        PsychErrorExit(PsychError_internal);

}

/*
    PsychReadNormalizedGammaTable()
    
    TO DO: This should probably be changed so that the caller allocates the memory.
    TO DO: Adopt a naming convention which distinguishes between functions which allocate memory for return variables from those which do not.
            For example, PsychReadNormalizedGammaTable() vs. PsychGetNormalizedGammaTable().
    
*/
void PsychReadNormalizedGammaTable(int screenNumber, int *numEntries, float **redTable, float **greenTable, float **blueTable)
{
    CGDirectDisplayID	cgDisplayID;
    static float localRed[1024], localGreen[1024], localBlue[1024];
    CGDisplayErr error; 
    CGTableCount sampleCount; 
        
    *redTable=localRed; *greenTable=localGreen; *blueTable=localBlue; 
    PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, screenNumber);
    error=CGGetDisplayTransferByTable(cgDisplayID, (CGTableCount)1024, *redTable, *greenTable, *blueTable, &sampleCount);
    *numEntries=(int)sampleCount;
}

void PsychLoadNormalizedGammaTable(int screenNumber, int numEntries, float *redTable, float *greenTable, float *blueTable)
{
    CGDisplayErr 	error; 
    CGDirectDisplayID	cgDisplayID;
    
    PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, screenNumber);
    error=CGSetDisplayTransferByTable(cgDisplayID, (CGTableCount)numEntries, redTable, greenTable, blueTable);
    
}

// PsychGetDisplayBeamPosition() contains the implementation of display beamposition queries.
// It requires both, a cgDisplayID handle, and a logical screenNumber and uses one of both for
// deciding which display pipe to query, whatever of both is more efficient or suitable for the
// host platform -- This is ugly, but neccessary, because the mapping with only one of these
// specifiers would be either ambigous (wrong results!) or usage would be inefficient and slow
// (bad for such a time critical low level call!). On some systems it may even ignore the arguments,
// because it's not capable of querying different pipes - ie., it will always query a hard-coded pipe.
//
// On MacOS/X, this is a simple wrapper around the OSX CoreGraphics call CGDisplayBeamPosition().
// That call is currently supported by all gfx-chips/drivers on old PowerPC based Macs. It's also
// supported for NVidia cards on the Intel based Macs, starting with OS/X 10.4.10 and later, 10.5
// and later. On IntelMacs with ATI or Intel gfx, the call returns 0 or -1 (unsupported).
int PsychGetDisplayBeamPosition(CGDirectDisplayID cgDisplayId, int screenNumber)
{	
	// First try standard, official Apple OS/X supported method:
	int beampos = -1;
	
	if (PsychPrefStateGet_ConserveVRAM() & kPsychDontUseNativeBeamposQuery) {
		// OS/X native beamposition queries forcefully disabled!
		// Try to use our own homegrown fallback solution:
		return(PsychOSKDGetBeamposition(screenNumber));
	}

	if (repeatedZeroBeamcount[screenNumber] == -20000) {
		// OS/X native beamposition queries verified to work: Use 'em:
		return((int) CGDisplayBeamPosition(cgDisplayId));
	}

	if (repeatedZeroBeamcount[screenNumber] == -10000) {
		// OS/X native beamposition queries verified to *not* work!
		// Try to use our own homegrown fallback solution:
		return(PsychOSKDGetBeamposition(screenNumber));
	}

	// At this point, we don't know yet if native beampos queries work. Use some
	// detection logic: First we start with assumption "native works"...
	beampos = CGDisplayBeamPosition(cgDisplayId);
	
	// ...then we try to verify that assumption:
	if (beampos > 0) {
		// They seem to work! Mark them permanently as operational:
		repeatedZeroBeamcount[screenNumber] = -20000;
		return(beampos);
	}

	// Totally unsupported?
	if (beampos == -1) {
		// Mark'em as unsupported and use our fallback:
		repeatedZeroBeamcount[screenNumber] = -10000;
		return(PsychOSKDGetBeamposition(screenNumber));
	}
	
	// We got a zero value. Could be by accident or by failure:
	
	// Worked? A failure is indicated by either value -1 (officially unsupported),
	// or a constant value zero. We use a counter array to check if multiple queries
	// returned a zero value:
	if ((repeatedZeroBeamcount[screenNumber]++) > 0) {
		// Second zero result in a row! Native queries don't work, mark them
		// as unsupported and use fallback:
		repeatedZeroBeamcount[screenNumber] = -10000;
		beampos = PsychOSKDGetBeamposition(screenNumber);
	}
	
	return(beampos);
}

// This will launch the OS/X "Console.app" so users can see the IOLogs from the KEXT.
void PsychLaunchConsoleApp(void)
{
	CFURLRef pathRef;

    pathRef = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR(kMyPathToSystemLog), kCFURLPOSIXPathStyle, false);    
    if (pathRef) {
        LSOpenCFURLRef(pathRef, NULL);
        CFRelease(pathRef);
    }

	return;
}

// Try to attach to kernel level ptb support driver and setup everything, if it works:
void InitPsychtoolboxKernelDriverInterface(void)
{
    kern_return_t	kernResult; 
    io_service_t	service;
    io_connect_t	connect;
    io_iterator_t 	iterator;
    CFDictionaryRef	classToMatch;
    int				i;
	
	// Reset to zero open drivers to start with:
	numKernelDrivers = 0;
	
    // This will launch the OS/X "Console.app" so users can see the IOLogs from the KEXT.
    if (false) PsychLaunchConsoleApp();

	// Setup matching criterion to find our driver in the IORegistry device tree:
	classToMatch = IOServiceMatching(kMyDriversIOKitClassName);
    if (classToMatch == NULL) {
        printf("PTB-DEBUG: IOServiceMatching() for Psychtoolbox kernel support driver returned a NULL dictionary. Kernel driver support disabled.\n");
        return;
    }
    
    // This creates an io_iterator_t of all instances of our driver that exist in the I/O Registry. Each installed graphics card
	// will get its own instance of a driver. The iterator allows to iterate over all instances:
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault, classToMatch, &iterator);
    if (kernResult != KERN_SUCCESS) {
        printf("PTB-DEBUG: IOServiceGetMatchingServices for Psychtoolbox kernel support driver returned 0x%08x. Kernel driver support disabled.\n", kernResult);
        return;
    }
        
    // In a polished final version we would want to handle the case where more than one gfx-card is attached.
	// The iterator would return multiple instances of our driver and we need to decide which one to connect to.
	// For now, we do not handle this case but instead just get the first item from the iterator.
    service = IOIteratorNext(iterator);
    
    // Release the io_iterator_t now that we're done with it.
    IOObjectRelease(iterator);
    
    if (service == IO_OBJECT_NULL) {
        // printf("PTB-INFO: Couldn't find an operative Psychtoolbox kernel support driver. Features based on kernel driver won't be available.\n");
		return;
    }
    else {
		// Instantiate a connection to the user client.

		// This call will cause the user client to be instantiated. It returns an io_connect_t handle
		// that is used for all subsequent calls to the user client.
		connect = 0;
		kernResult = IOServiceOpen(service, mach_task_self(), 0, &connect);
		if (kernResult != KERN_SUCCESS) {
			printf("PTB-DEBUG: IOServiceOpen for our driver returned 0x%08x. Kernel driver support disabled.\n", kernResult);
		}
		else {
			// This is an example of calling our user client's openUserClient method.
			kernResult = IOConnectMethodScalarIScalarO(connect, kMyUserClientOpen, 0, 0);
			if (kernResult == KERN_SUCCESS) {
				if (false) printf("PTB-DEBUG: IOConnectMethodScalarIScalarO was successful.\n\n");
			}
			else {
				// Release connection:
				IOServiceClose(connect);
				connect = IO_OBJECT_NULL;
				printf("PTB-DEBUG: IOConnectMethodScalarIScalarO for our driver returned 0x%08x. Kernel driver support disabled.\n", kernResult);
				if (kernResult == kIOReturnExclusiveAccess) printf("PTB-DEBUG: Please check if other applications (e.g., other open Matlabs?) use the driver already.\n");
			}
		}

		// Release the io_service_t now that we're done with it.
		IOObjectRelease(service);

		if (connect != IO_OBJECT_NULL) {
			// Final success!
			printf("PTB-INFO: Connection to Psychtoolbox kernel support driver established. Will use the driver...\n\n");

			// For now, as we only support one gfx-card, just init all screens handles to the
			// single connect handle we got:
			for(i=0; i< kPsychMaxPossibleDisplays; i++){
				displayConnectHandles[i]=connect;
			}
			
			// Increment instance count by one:
			numKernelDrivers++;
		}
		else {
			// Final failure:
			printf("PTB-DEBUG: IOConnectMethodScalarIScalarO for our driver returned IO_OBJECT_NULL connect handle. Kernel driver support disabled.\n");
		}
	}
	
	// Done.
	return;
}

// Try to detach to kernel level ptb support driver and tear down everything:
void PsychOSShutdownPsychtoolboxKernelDriverInterface(void)
{
	io_connect_t connect;
	
	if (numKernelDrivers > 0) {
		// Currently we only support one graphics card, so just take
		// the connect handle of first screen:
		connect = displayConnectHandles[0];

		// Call shutdown method:
		kern_return_t kernResult = IOConnectMethodScalarIScalarO(connect, kMyUserClientClose, 0, 0);
		if (kernResult == KERN_SUCCESS) {
			if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: IOConnectMethodScalarIScalarO Closing was successfull.\n");
		}
		else {
			if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: IOConnectMethodScalarIScalarO Closing failed with kernel return code 0x%08x.\n\n", kernResult);
		}
		
		// Close IOService:
		kernResult = IOServiceClose(connect);
		if (kernResult == KERN_SUCCESS) {
			if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: IOServiceClose() was successfull.\n");
		}
		else {
			if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: IOServiceClose returned 0x%08x\n\n", kernResult);
		}		
	}

	// Ok, whatever happened, we're detached (for good or bad):
	numKernelDrivers = 0;

	return;
}

psych_bool PsychOSIsKernelDriverAvailable(int screenId)
{
	return((displayConnectHandles[screenId]) ? TRUE : FALSE);
}

io_connect_t PsychOSCheckKDAvailable(int screenId, unsigned int * status)
{
	io_connect_t connect = displayConnectHandles[screenId];

	if (numKernelDrivers<=0) {
		if (status) *status = kIOReturnNotFound;
		return(0);
	}
	
	if (!connect) {
		if (status) *status = kIOReturnNoDevice;
		if (PsychPrefStateGet_Verbosity() > 6) printf("PTB-DEBUGINFO: Could not access kernel driver connection for screenId %i - No such connection.\n", screenId);
		return(0);
	}

	if (status) *status = kIOReturnSuccess;
	return(connect);
}

kern_return_t PsychOSKDDispatchCommand(io_connect_t connect, const PsychKDCommandStruct* inStruct, PsychKDCommandStruct* outStruct, unsigned int* status)
{
    IOByteCount structOSize = sizeof(PsychKDCommandStruct);
	kern_return_t kernResult =	IOConnectMethodStructureIStructureO(connect,							// an io_connect_t returned from IOServiceOpen().
																	kPsychKDDispatchCommand,			// an index to the function to be called via the user client.
																	sizeof(PsychKDCommandStruct),		// the size of the input struct paramter.
																	&structOSize,						// a pointer to the size of the output struct paramter.
																	(PsychKDCommandStruct*)inStruct,	// a pointer to the input struct parameter.
																	outStruct							// a pointer to the output struct parameter.
																	);
    
	if (status) *status = kernResult;
	if (kernResult != kIOReturnSuccess) {
		if (PsychPrefStateGet_Verbosity() > 0) printf("PTB-ERROR: Kernel driver command dispatch failure for code %lx (Kernel error code: %lx).\n", inStruct->command, kernResult);
	}

    return kernResult;	
}

unsigned int PsychOSKDReadRegister(int screenId, unsigned int offset, unsigned int* status)
{
	// Have syncCommand locally defined, ie. on threads local stack: Important for thread-safety, e.g., for async-flip etc.:
	PsychKDCommandStruct syncCommand;
	
	// Check availability of connection:
	io_connect_t connect;
	if (!(connect = PsychOSCheckKDAvailable(screenId, status))) return(0xffffffff);

	// Set command code for register read:
	syncCommand.command = kPsychKDReadRegister;

	// Register offset is arg 0:
	syncCommand.inOutArgs[0] = offset;
	
	// Issue request to driver:
	kern_return_t kernResult = PsychOSKDDispatchCommand(connect, &syncCommand, &syncCommand, status);    
	if (kernResult != KERN_SUCCESS) {
		if (PsychPrefStateGet_Verbosity() > 0) printf("PTB-ERROR: Kernel driver register read failed for register %lx (Kernel error code: %lx).\n", offset, kernResult);
		// A value of 0xffffffff signals failure:
		return(0xffffffff);
	}
	
	// Return readback register value:
	return((int) syncCommand.inOutArgs[0]);
}

unsigned int PsychOSKDWriteRegister(int screenId, unsigned int offset, unsigned int value, unsigned int* status)
{
	// Have syncCommand locally defined, ie. on threads local stack: Important for thread-safety, e.g., for async-flip etc.:
	PsychKDCommandStruct syncCommand;

	// Check availability of connection:
	io_connect_t connect;
	if (!(connect = PsychOSCheckKDAvailable(screenId, status))) return(0xffffffff);

	// Set command code for display sync:
	syncCommand.command = KPsychKDWriteRegister;
	syncCommand.inOutArgs[0] = offset;
	syncCommand.inOutArgs[1] = value;
	
	// Issue request to driver:
	kern_return_t kernResult = PsychOSKDDispatchCommand(connect, &syncCommand, &syncCommand, status);    
	if (kernResult != KERN_SUCCESS) {
		if (PsychPrefStateGet_Verbosity() > 0) printf("PTB-ERROR: Kernel driver register write failed for register %lx, value %lx (Kernel error code: %lx).\n", offset, value, kernResult);
		// A value of 0xffffffff signals failure:
		return(0xffffffff);
	}
	
	// Return success:
	return(0);
}

// Synchronize display screens video refresh cycle. See PsychSynchronizeDisplayScreens() for help and details...
PsychError PsychOSSynchronizeDisplayScreens(int *numScreens, int* screenIds, int* residuals, unsigned int syncMethod, double syncTimeOut, int allowedResidual)
{
	// Have syncCommand locally defined, ie. on threads local stack: Important for thread-safety, e.g., for async-flip etc.:
	PsychKDCommandStruct syncCommand;

	int screenId = 0;
	double	abortTimeOut, now;
	int residual;
	
	// Check availability of connection:
	io_connect_t connect;
	unsigned int status;
	kern_return_t kernResult;
	
	// No support for other methods than fast hard sync:
	if (syncMethod > 1) {
		if (PsychPrefStateGet_Verbosity() > 1) printf("PTB-WARNING: Could not execute display resync operation with requested non hard sync method. Not supported for this setup and settings.\n"); 
		return(PsychError_unimplemented);
	}
	
	// The current implementation only supports syncing all heads of a single card
	if (*numScreens <= 0) {
		// Resync all displays requested: Choose screenID zero for connect handle:
		screenId = 0;
	}
	else {
		// Resync of specific display requested: We only support resync of all heads of a single multi-head card,
		// therefore just choose the screenId of the passed master-screen for resync handle:
		screenId = screenIds[0];
	}
	
	if (!(connect = PsychOSCheckKDAvailable(screenId, &status))) {
		if (PsychPrefStateGet_Verbosity() > 1) printf("PTB-WARNING: Could not execute display resync operation for master screenId %i. Not supported for this setup and settings.\n", screenId); 
		return(PsychError_unimplemented);
	}
	
	// Setup deadline for abortion or repeated retries:
	PsychGetAdjustedPrecisionTimerSeconds(&abortTimeOut);
	abortTimeOut+=syncTimeOut;
	residual = INT_MAX;
	
	// Repeat until timeout or good enough result:
	do {
		// If this isn't the first try, wait 0.5 secs before retry:
		if (residual != INT_MAX) PsychWaitIntervalSeconds(0.5);
		
		residual = INT_MAX;

		// Set command code for display sync:
		syncCommand.command = kPsychKDFastSyncAllHeads;
		
		// Issue request to driver:
		kernResult = PsychOSKDDispatchCommand(connect, &syncCommand, &syncCommand, &status);
		if (kernResult == KERN_SUCCESS) {
			residual = (int) syncCommand.inOutArgs[0];
			if (PsychPrefStateGet_Verbosity() > 2) printf("PTB-INFO: Graphics display heads resynchronized. Residual vertical beamposition error is %ld scanlines.\n", residual);
		}
		else {
			if (PsychPrefStateGet_Verbosity() > 0) printf("PTB-ERROR: Graphics display head synchronization failed (Kernel error code: %lx).\n", kernResult);
			break;
		}
		
		// Timestamp:
		PsychGetAdjustedPrecisionTimerSeconds(&now);
	} while ((now < abortTimeOut) && (abs(residual) > allowedResidual) && (kernResult == KERN_SUCCESS));

	// Return residual value if wanted:
	if (residuals) { 
		residuals[0] = residual;
	}
	
	if (abs(residual) > allowedResidual) {
		if (PsychPrefStateGet_Verbosity() > 1) printf("PTB-WARNING: Failed to synchronize heads down to the allowable residual of +/- %i scanlines. Final residual %i lines.\n", allowedResidual, residual);
	}
	
	// TODO: Error handling not really worked out...
	if (residual == INT_MAX || kernResult != KERN_SUCCESS) return(PsychError_system);
	
	// Done.
	return(PsychError_none);
}

int PsychOSKDGetBeamposition(int screenId)
{
	// Have syncCommand locally defined, ie. on threads local stack: Important for thread-safety, e.g., for async-flip etc.:
	PsychKDCommandStruct syncCommand;

	// Check availability of connection:
	io_connect_t connect;
	if (!(connect = PsychOSCheckKDAvailable(screenId, NULL))) {
		// Beampos queries unavailable:
		if (PsychPrefStateGet_Verbosity() > 11) printf("PTB-DEBUG: Kernel driver based beamposition queries unavailable for screenId %i.\n", screenId);
		return(-1);
	}
	
	// Set command code for beamposition query:
	syncCommand.command = kPsychKDGetBeamposition;
	
	// Assign headid for this screen: Currently we only support two display heads and
	// statically assign headid 0 to screenid 0 and headid 1 to screenid 1.
	// For now, we hope that PTB's high-level consistency checking will detect mismapped
	// queries and disable beamposition queries in that case.
	// TODO: Implement a more powerful and flexible mapping!
	syncCommand.inOutArgs[0] = (screenId > 0) ? 1 : 0;
	
	// Issue request:
	kern_return_t kernResult = PsychOSKDDispatchCommand(connect,  &syncCommand, &syncCommand, NULL);    
	if (kernResult != KERN_SUCCESS) {
		if (PsychPrefStateGet_Verbosity() > 6) printf("PTB-ERROR: Kernel driver beamposition query failed (Kernel error code: %lx).\n", kernResult);
		// A value of -1 signals beamposition queries unsupported or invalid:
		return(-1);
	}

	// Return queried position:
	return((int) syncCommand.inOutArgs[0]);
}
