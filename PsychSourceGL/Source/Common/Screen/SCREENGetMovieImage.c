/*
Psychtoolbox3/Source/Common/SCREENGetMovieImage.c		
 
 AUTHORS:
 mario.kleiner at tuebingen.mpg.de   mk
 
 PLATFORMS:	
 This file should build on any platform. 
 
 HISTORY:
 10/23/05  mk		Created. 
 
 DESCRIPTION:
 
 Fetch an image from the specified movie object and create an OpenGL texture out of it.
 Return a handle to the texture.
 
 On OS-X and Windows, all movie/multimedia handling functions are implemented via the Apple Quicktime API,
 version 6 or later. 

 TO DO:
 
 */

#include "Screen.h"

static char useString[] = "[ texturePtr [timeindex]]=Screen('GetMovieImage', windowPtr, moviePtr, [waitForImage=1], [fortimeindex], [specialFlags = 0]);";
static char synopsisString[] = 
"Try to fetch a new texture image from movie object 'moviePtr' for visual playback/display in onscreen window 'windowPtr' and "
"return a texture-handle 'texturePtr' on successfull completion. 'waitForImage' If set to 1 (default), the function will wait "
"until the image becomes available. If set to zero, the function will just poll for a new image. If none is ready, it will return "
"a texturePtr of zero, or -1 if none will become ready because movie has reached its end and is not in loop mode. \n"
"'fortimeindex' Don't request the next image, but the image closest to time 'fortimeindex' in seconds. The (optional) return "
"value 'timeindex' contains the exact time when the returned image should be displayed wrt. to the start of the movie - a presentation timestamp. \n"
"'specialFlags' (optional) encodes special requirements for the returned texture. A setting of 2 will request that the texture "
"is prepared for drawing it with highest possible precision. See explanation of 'specialFlags' == 2 in Screen('MakeTexture') "
"for details. \n"
"On OS-X and Windows, media files are handled by use of Apples Quicktime-7 API. On other platforms, the playback engine may be different "
"from Quicktime.";
static char seeAlsoString[] = "CloseMovie PlayMovie GetMovieImage GetMovieTimeIndex SetMovieTimeIndex";

PsychError SCREENGetMovieImage(void) 
{
    PsychWindowRecordType		*windowRecord;
    PsychWindowRecordType		*textureRecord;
    PsychRectType				rect;
    int                         moviehandle = -1;
    int                         waitForImage = TRUE;
    double                      requestedTimeIndex = -1;
    double                      presentation_timestamp = 0;
    int							rc=0;
    int							specialFlags = 0;
	
    // All sub functions should have these two lines
    PsychPushHelp(useString, synopsisString, seeAlsoString);
    if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none);};
    
    PsychErrorExit(PsychCapNumInputArgs(5));            // Max. 4 input args.
    PsychErrorExit(PsychRequireNumInputArgs(2));        // Min. 2 input args required.
    PsychErrorExit(PsychCapNumOutputArgs(2));           // Max. 2 output args.
    
    // Get the window record from the window record argument and get info from the window record
    PsychAllocInWindowRecordArg(kPsychUseDefaultArgPosition, TRUE, &windowRecord);
    // Only onscreen windows allowed:
    if(!PsychIsOnscreenWindow(windowRecord)) {
        PsychErrorExitMsg(PsychError_user, "GetMovieImage called on something else than an onscreen window.");
    }
    
    // Get the movie handle:
    PsychCopyInIntegerArg(2, TRUE, &moviehandle);
    if (moviehandle==-1) {
        PsychErrorExitMsg(PsychError_user, "GetMovieImage called without valid handle to a movie object.");
    }

    // Get the 'waitForImage' flag: If waitForImage == true == 1, we'll do a blocking wait for
    // arrival of a new image for playback. Otherwise we will return with a 0-Handle if there
    // isn't any new image available.
    PsychCopyInIntegerArg(3, FALSE, &waitForImage);
    
    // Get the requested timeindex for the frame. The default is -1, which means: Get the next image,
    // according to current movie playback time.
    PsychCopyInDoubleArg(4, FALSE, &requestedTimeIndex);
    
    // Get the optional specialFlags flag:
    PsychCopyInIntegerArg(5, FALSE, &specialFlags);

    while (rc==0) {
        rc = PsychGetTextureFromMovie(windowRecord, moviehandle, TRUE, requestedTimeIndex, NULL, NULL);
        if (rc<0) {
            // No image available and there won't be any in the future, because the movie has reached
            // its end and we are not in looped playback mode:

            // No new texture available: Return a negative handle:
            PsychCopyOutDoubleArg(1, TRUE, -1);
            // ...and an invalid timestamp:
            PsychCopyOutDoubleArg(2, FALSE, -1);
            // Ready!
            return(PsychError_none);
        }
        else if (rc==0 && waitForImage == 0) {
            // We should just poll once and no new texture available: Return a null-handle:
            PsychCopyOutDoubleArg(1, TRUE, 0);
            // ...and an invalid timestamp:
            PsychCopyOutDoubleArg(2, FALSE, -1);
            // Ready!
            return(PsychError_none);
        }
        else if (rc==0 && waitForImage != 0) {
            // No new texture available yet. Just sleep a bit and then retry...
            PsychWaitIntervalSeconds(0.005);
        }
    }

    // New image available: Go ahead...
    
    // Create a texture record.  Really just a window record adapted for textures.  
    PsychCreateWindowRecord(&textureRecord);	// This also fills the window index field.
    // Set mode to 'Texture':
    textureRecord->windowType=kPsychTexture;
    // We need to assign the screen number of the onscreen-window.
    textureRecord->screenNumber=windowRecord->screenNumber;
    // It is always a 32 bit texture for movie textures:
    textureRecord->depth=32;
	textureRecord->nrchannels = 4;

    // Create default rectangle which describes the dimensions of the image. Will be overwritten
    // later on.
    PsychMakeRect(rect, 0, 0, 10, 10);
    PsychCopyRect(textureRecord->rect, rect);
    
    // Other setup stuff:
    textureRecord->textureMemorySizeBytes= 0;
    textureRecord->textureMemory=NULL;

	// Assign parent window and copy its inheritable properties:
	PsychAssignParentWindow(textureRecord, windowRecord);

    // Try to fetch an image from the movie object and return it as texture:
    PsychGetTextureFromMovie(windowRecord, moviehandle, FALSE, requestedTimeIndex, textureRecord, &presentation_timestamp);

	// Assign GLSL filter-/lookup-shaders if needed: usefloatformat is always == 0 as
	// our current movie engine implementations only return 8 bpc fixed textures.
	// The 'userRequest' flag is set if specialmode flag is set to 8.
	PsychAssignHighPrecisionTextureShaders(textureRecord, windowRecord, 0, (specialFlags & 2) ? 1 : 0);

    // Texture ready for consumption. Mark it valid and return handle to userspace:
    PsychSetWindowRecordValid(textureRecord);
    PsychCopyOutDoubleArg(1, TRUE, textureRecord->windowIndex);
    // Return presentation timestamp for this image:
    PsychCopyOutDoubleArg(2, FALSE, presentation_timestamp);
    
    // Ready!
    return(PsychError_none);
}
