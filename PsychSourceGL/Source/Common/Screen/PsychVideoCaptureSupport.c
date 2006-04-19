/*
	Common/Screen/PsychVideoCaptureSupport.c
	
	PLATFORMS:	
	
		This is the OS independent version (for now: Should work on OS-X and Windows)  
		A GNU/Linux specific version is stored in the /Linux/ folder. It has the
		same API - and therefore the same header file, but a pretty different
		implementation.

	AUTHORS:
	
		Mario Kleiner           mk              mario.kleiner@tuebingen.mpg.de

	HISTORY:
	
        DESCRIPTION:
	
		Psychtoolbox functions for dealing with video capture devices.
	
	NOTES:

*/


#include "Screen.h"
#include <float.h>

// Linux support is implemented in the ../Linux/Screen/... subfolder of Psychtoolbox.
#if PSYCH_SYSTEM != PSYCH_LINUX

#if PSYCH_SYSTEM == PSYCH_OSX
#include <Quicktime/QuickTimeComponents.h>
#endif

#if PSYCH_SYSTEM == PSYCH_WINDOWS
#include <QTML.h>
#include <QuickTimeComponents.h>
#endif

#define PSYCH_MAX_CAPTUREDEVICES 10

// Record which defines all state for a capture device:
typedef struct {
    GWorldPtr           gworld;       // Offscreen GWorld into which captured frame is decompressed.
    SeqGrabComponent 	seqGrab;	     // Sequence grabber handle.
    SGChannel           sgchanVideo;  // Handle for video channel of sequence grabber.
    ImageSequence 	decomSeq;	     // unique identifier for our video decompression sequence
    int nrframes;                     // Total count of decompressed images.
    double fps;                       // Acquisition framerate of capture device.
    int width;                        // Width x height of captured images.
    int height;
    double last_pts;                  // Capture timestamp of previous frame.
    double current_pts;               // Capture timestamp of current frame
    int nr_droppedframes;             // Counter for dropped frames.
    int frame_ready;                  // Signals availability of new frames for conversion into GL-Texture.
    int grabber_active;               // Grabber running?
    Rect roirect;                     // Region of interest rectangle - denotes subarea of full video capture area.
    double avg_decompresstime;        // Average time spent in Quicktime/Sequence Grabber decompressor.
    double avg_gfxtime;               // Average time spent in GWorld --> OpenGL texture conversion and statistics.
    int nrgfxframes;                  // Count of fetched textures.
} PsychVidcapRecordType;

static PsychVidcapRecordType vidcapRecordBANK[PSYCH_MAX_CAPTUREDEVICES];
static int numCaptureRecords = 0;
static Boolean firsttime = TRUE;

/** PsychVideoCaptureDataProc
 *  This callback is called by the SequenceGrabber subsystem whenever a new frame arrives from
 *  the framegrabber hardware. It performs first-time setup of decompression sequence/codecs and
 *  decompression/conversion of grabbed raw data into a bitmap image -> GWorld.
 */
OSErr PsychVideoCaptureDataProc(SGChannel c, Ptr p, long len, long *offset, long chRefCon, TimeValue time, short writeType, long refCon)
{
    int handle;
    OSErr err = noErr;
    CodecFlags	ignore;
    TimeScale 	timeScale;
    double tstart, tend;
    
    PsychGetAdjustedPrecisionTimerSeconds(&tstart);
    
    // Retrieve handle to our capture data structure:
    handle = (int) chRefCon;

    // Compute capture timestamp:
    err = SGGetChannelTimeScale(c, &timeScale);
    vidcapRecordBANK[handle].current_pts = (double) time / (double) timeScale;
    
    // GWorld for offscreen rendering available?
    if (vidcapRecordBANK[handle].gworld && vidcapRecordBANK[handle].grabber_active) {
        // Yes.
        
        // First time invocation for this sequence grabber?
        if (vidcapRecordBANK[handle].decomSeq == 0) {
            // Need to do one-time setup of decompression sequence:
            ImageDescriptionHandle imageDesc = (ImageDescriptionHandle) NewHandle(0);
            
            // retrieve a channel�s current sample description, the channel returns a sample description that is
            // appropriate to the type of data being captured
            err = SGGetChannelSampleDescription(c, (Handle)imageDesc);
                        
            // begin the process of decompressing a sequence of frames
            // this is a set-up call and is only called once for the sequence - the ICM will interrogate different codecs
            // and construct a suitable decompression chain, as this is a time consuming process we don't want to do this
            // once per frame (eg. by using DecompressImage)
            // for more information see Ice Floe #8 http://developer.apple.com/quicktime/icefloe/dispatch008.html
            // the destination is specified as the GWorld
            err = DecompressSequenceBegin(&(vidcapRecordBANK[handle].decomSeq),	// pointer to field to receive unique ID for sequence
                                          imageDesc,                            // handle to image description structure
                                          vidcapRecordBANK[handle].gworld,      // port for the DESTINATION image
                                          NULL,					// graphics device handle, if port is set, set to NULL
                                          &(vidcapRecordBANK[handle].roirect),	// source rectangle defining the portion of the image to decompress 
                                          NULL,                                 // transformation matrix
                                          srcCopy,				// transfer mode specifier
                                          (RgnHandle)NULL,                      // clipping region in dest. coordinate system to use as a mask
                                          0,					// flags
                                          codecNormalQuality,                   // accuracy in decompression
                                          bestSpeedCodec);                      // compressor identifier or special identifiers ie. bestSpeedCodec
            if (err!=noErr) {
                printf("PTB-ERROR: Error in Video capture callback!!!\n");
                fflush(NULL);
            }
            
            DisposeHandle((Handle)imageDesc);         
            // printf("DECOMPRESS-ONE_TIME!\n"); fflush(NULL);
            // One-Time setup of decompression engine done.
        }
        
        // Decompress new frame into our offscreen GWorld:
        err = DecompressSequenceFrameS(vidcapRecordBANK[handle].decomSeq,	// sequence ID returned by DecompressSequenceBegin
                                       p,					// pointer to compressed image data
                                       len,					// size of the buffer
                                       0,					// in flags
                                       &ignore,				// out flags
                                       NULL);				// async completion proc
        
        if (err!=noErr) {
            printf("PTB-ERROR: Error in Video capture callback code %i!!!\n", (int) err);
            fflush(NULL);
        }
        
        // Now we should have the required texture data in our GWorld...
        // Increment the newimage - flag:
        vidcapRecordBANK[handle].frame_ready++;
    
        PsychGetAdjustedPrecisionTimerSeconds(&tend);
        vidcapRecordBANK[handle].avg_decompresstime+=(tend - tstart);
        // Update framecounter:
        vidcapRecordBANK[handle].nrframes++;
    }
    
    
    return(noErr);
}

/*
 *     PsychVideoCaptureInit() -- Initialize video capture subsystem.
 *     This routine is called by Screen's RegisterProject.c PsychModuleInit()
 *     routine at Screen load-time. It clears out the vidcapRecordBANK to
 *     bring the subsystem into a clean initial state.
 */
void PsychVideoCaptureInit(void)
{
    // Initialize vidcapRecordBANK with NULL-entries:
    int i;
    for (i=0; i < PSYCH_MAX_CAPTUREDEVICES; i++) {
        vidcapRecordBANK[i].gworld = (GWorldPtr) NULL;
        vidcapRecordBANK[i].seqGrab = (SeqGrabComponent) NULL;
        vidcapRecordBANK[i].decomSeq = 0;
        vidcapRecordBANK[i].grabber_active = 0;
    }    
    numCaptureRecords = 0;
    
    return;
}

/*
 *      PsychOpenVideoCaptureDevice() -- Create a video capture object.
 *
 *      This function tries to open a Quicktime-Sequencegrabber
 *      and return the associated captureHandle for it.
 *
 *      win = Pointer to window record of associated onscreen window.
 *      deviceIndex = Index of the grabber device. (Currently ignored)
 *      capturehandle = handle to the new capture object.
 *      capturerectangle = If non-NULL a ptr to a PsychRectangle which contains the ROI for capture.
 *      The following arguments are currently ignored on Windows and OS-X:
 *      reqdepth = Number of layers for captured output textures. (0=Don't care, 1=LUMINANCE8, 2=LUMINANCE8_ALPHA8, 3=RGB8, 4=RGBA8)
 *      num_dmabuffers = Number of buffers in the ringbuffer queue (e.g., DMA buffers) - This is OS specific. Zero = Don't care.
 *      allow_lowperf_fallback = If set to 1 then PTB can use a slower, low-performance fallback path to get nasty devices working.
 */
bool PsychOpenVideoCaptureDevice(PsychWindowRecordType *win, int deviceIndex, int* capturehandle, double* capturerectangle,
				 int reqdepth, int num_dmabuffers, int allow_lowperf_fallback)
{
    int i, slotid;
    OSErr error;
    char msgerr[10000];
    char errdesc[1000];
    Rect movierect, newrect;
    SeqGrabComponent seqGrab = NULL;
    SGChannel *sgchanptr = NULL;
    ImageDescriptionHandle imageDesc;
    Fixed framerate;
    *capturehandle = -1;
    error=noErr;

    // We startup the Quicktime subsystem only on first invocation.
    if (firsttime) {
#if PSYCH_SYSTEM == PSYCH_WINDOWS
        // Initialize Quicktime for Windows compatibility layer: This will fail if
        // QT isn't installed on the Windows machine...
        error = InitializeQTML(0);
        if (error!=noErr) {
            PsychErrorExitMsg(PsychError_internal, "Quicktime Media Layer initialization failed: Quicktime not properly installed?!?");
        }
#endif

        // Initialize Quicktime-Subsystem:
        error = EnterMovies();
        if (error!=noErr) {
            PsychErrorExitMsg(PsychError_internal, "Quicktime EnterMovies() failed!!!");
        }
        firsttime = FALSE;
    }
    
    if (!PsychIsOnscreenWindow(win)) {
        PsychErrorExitMsg(PsychError_user, "Provided windowPtr is not an onscreen window.");
    }

    if (deviceIndex < 0) {
        PsychErrorExitMsg(PsychError_internal, "Invalid (negative) deviceIndex passed!");
    }

    if (numCaptureRecords >= PSYCH_MAX_CAPTUREDEVICES) {
        PsychErrorExitMsg(PsychError_user, "Allowed maximum number of simultaneously open capture devices exceeded!");
    }

    // Search first free slot in vidcapRecordBANK:
    for (i=0; (i < PSYCH_MAX_CAPTUREDEVICES) && (vidcapRecordBANK[i].gworld); i++);
    if (i>=PSYCH_MAX_CAPTUREDEVICES) {
        PsychErrorExitMsg(PsychError_user, "Allowed maximum number of simultaneously open capture devices exceeded!");
    }

    // Slot slotid will contain the record for our new capture object:
    slotid=i;
    
    // Zero-out new record:
    vidcapRecordBANK[slotid].gworld=NULL;
    vidcapRecordBANK[slotid].decomSeq=0;    
    vidcapRecordBANK[slotid].grabber_active = 0;
        
    // Open sequence grabber:
    // ======================
    
    // Open the default sequence grabber: The deviceIndex is currently ignored.
    seqGrab = OpenDefaultComponent(SeqGrabComponentType, 0);
    if (seqGrab == NULL) {
        PsychErrorExitMsg(PsychError_internal, "Failed to open the default sequence grabber for video capture!");
    }
    
    // Initialize the sequence grabber component:
    error=noErr;

    error = SGInitialize(seqGrab);
    if (error != noErr) {
        if (seqGrab) CloseComponent(seqGrab);
        PsychErrorExitMsg(PsychError_internal, "SGInitialize() for capture device failed!"); 
    }

    // Specify the destination data reference for a record operation
    // tell it we're not making a movie
    // if the flag seqGrabDontMakeMovie is used, the sequence grabber still calls
    // your data function, but does not write any data to the movie file
    // writeType will always be set to seqGrabWriteAppend
    error = SGSetDataRef(seqGrab, 0, 0, seqGrabDontMakeMovie);
    if (error !=noErr) {
        if (seqGrab) CloseComponent(seqGrab);
        PsychErrorExitMsg(PsychError_internal, "SGSetDataRef for capture device failed!");            
    }

	 // Set dummy GWorld - we need this to prevent SGNewChannel from crashing on Windoze.
	 SGSetGWorld(seqGrab, 0, 0);

    // Create and setup video channel on sequence grabber:
    sgchanptr = &(vidcapRecordBANK[slotid].sgchanVideo);
    error = SGNewChannel(seqGrab, VideoMediaType, sgchanptr);
    if (error == noErr) {
        // Retrieve size of the capture rectangle - and therefore size of
        // our GWorld for offscreen rendering:
        SGGetSrcVideoBounds(*sgchanptr, &movierect);
        
        // Capture-Rectangle (ROI) specified?
        if (capturerectangle) {
            // Yes. Try to set it up...
            // Assign new requested ROI:
            newrect.left=(short) capturerectangle[kPsychLeft];
            newrect.top=(short)  capturerectangle[kPsychTop];
            newrect.right=(short)  capturerectangle[kPsychRight];
            newrect.bottom=(short) capturerectangle[kPsychBottom];
            
            printf("PTB-INFO: Selected video capture ROI is %i,%i,%i,%i\n", newrect.left, newrect.top, newrect.right, newrect.bottom);
            
            if ((int) capturerectangle[kPsychLeft]<movierect.left || (int) capturerectangle[kPsychTop]<movierect.top ||
                (int) capturerectangle[kPsychRight]>movierect.right || (int) capturerectangle[kPsychBottom]>movierect.bottom) {
                // ROI is not a subrectangle of video digitizers maximum ROI!
                SGDisposeChannel(seqGrab, *sgchanptr);
                *sgchanptr = NULL;
                CloseComponent(seqGrab);
                PsychErrorExitMsg(PsychError_user, "Invalid video region of interest (not inside image) specified!");
            }
            
            // Try to set our own custom video capture rectangle for the digitizer hardware:
            error=SGSetVideoRect(*sgchanptr, &newrect);
            if (error!=noErr) {
                // Grabber didn't accept new rectangle :(
                printf("PTB-INFO: Video capture device didn't accept new capture area. Reverting to full hardware capture area,\n");
                printf("PTB-INFO: Trying to only process specified ROI by restricting conversion to ROI in software...\n");
                movierect.left=(int) capturerectangle[kPsychLeft];
                movierect.top=(int) capturerectangle[kPsychTop];
                movierect.right=(int) capturerectangle[kPsychRight];
                movierect.bottom=(int) capturerectangle[kPsychBottom];
            }
            else {
                // Retrieve new capture rectangle settings:
                error = SGGetVideoRect(*sgchanptr, &movierect);
            }
        }
        
        // Store our roirect in structure:
        vidcapRecordBANK[slotid].roirect = movierect;
        movierect.right-=movierect.left;
        movierect.bottom-=movierect.top;
        movierect.left=0;
        movierect.top=0;
        
        // Now that we know the movierect of our ROI,
        // destroy the channel, assign a properly sized GWorld
        // and recreate the channel:
        SGDisposeChannel(seqGrab, *sgchanptr);
        *sgchanptr = NULL;

        // Create GWorld for this grabber object:
        error = QTNewGWorld(&vidcapRecordBANK[slotid].gworld, 0, &movierect,  NULL, NULL, 0);
        if (error!=noErr) {
          CloseComponent(seqGrab);
          PsychErrorExitMsg(PsychError_internal, "Quicktime GWorld creation for capture device failed!");
        }
    
        // Set grabbers graphics world to our GWorld:
        error = SGSetGWorld(seqGrab, vidcapRecordBANK[slotid].gworld, NULL );
        if (error !=noErr) {
          DisposeGWorld(vidcapRecordBANK[slotid].gworld);
          vidcapRecordBANK[slotid].gworld = NULL;
          CloseComponent(seqGrab);
          PsychErrorExitMsg(PsychError_internal, "Assignment of GWorld to capture device failed!");            
        }

        // Create and setup video channel on sequence grabber:
        error = SGNewChannel(seqGrab, VideoMediaType, sgchanptr);
        if (error !=noErr) {
          DisposeGWorld(vidcapRecordBANK[slotid].gworld);
          vidcapRecordBANK[slotid].gworld = NULL;
          CloseComponent(seqGrab);
          PsychErrorExitMsg(PsychError_internal, "Assignment of GWorld to capture device failed!");            
        }

        // Try to set our own custom video capture rectangle:
        error=SGSetVideoRect(*sgchanptr, &movierect);
        if (error!=noErr) {
            // Grabber didn't accept new rectangle :(
            printf("PTB-WARNING: Video capture device didn't accept new capture area. Reverting to full area...\n"); fflush(NULL);
        }

        error = SGSetChannelBounds(*sgchanptr, &movierect);
        if (error == noErr) {
            // set usage for new video channel to avoid playthrough
            // note we don't set seqGrabPlayDuringRecord
            error = SGSetChannelUsage(*sgchanptr, seqGrabRecord | seqGrabLowLatencyCapture);
        }
        
        //if (error==noErr) error = SGGetChannelBounds(*sgchanptr, &movierect);

        if (error != noErr) {
            // clean up on failure
            SGDisposeChannel(seqGrab, *sgchanptr);
            *sgchanptr = NULL;
            DisposeGWorld(vidcapRecordBANK[slotid].gworld);
            vidcapRecordBANK[slotid].gworld = NULL;
            if (seqGrab) CloseComponent(seqGrab);
            PsychErrorExitMsg(PsychError_internal, "SGSetChannelBounds() or SGSetChannelUsage() for capture device failed!");            
        }
    }
    else {
        // clean up on failure
        *sgchanptr = NULL;
        if (seqGrab) CloseComponent(seqGrab);
        
        if (error == -9400 || error== -9405) PsychErrorExitMsg(PsychError_user, "Couldn't connect to video capture device! Device offline or disconnected?");
        PsychErrorExitMsg(PsychError_internal, "SGNewChannel() for capture device failed!");            
    }

    // Retrieve a channels current sample description, the channel returns a sample description that is
    // appropriate to the type of data being captured
    //    imageDesc = (ImageDescriptionHandle) NewHandle(0);
    //    error = SGGetChannelSampleDescription(vidcapRecordBANK[slotid].sgchanVideo, (Handle)imageDesc);
    //    movierect.left = 0;
    //    movierect.top = 0;
    //    movierect.right = (**imageDesc).width;
    //    movierect.bottom = (**imageDesc).height 
    //    DisposeHandle((Handle)imageDesc);
    
    // Specify a data callback function: Gets called whenever a new frame is ready...
    error = SGSetDataProc(seqGrab, NewSGDataUPP(PsychVideoCaptureDataProc), 0);
    if (error !=noErr) {
        DisposeGWorld(vidcapRecordBANK[slotid].gworld);
        vidcapRecordBANK[slotid].gworld = NULL;
        SGDisposeChannel(seqGrab, *sgchanptr);
        *sgchanptr = NULL;
        CloseComponent(seqGrab);
        PsychErrorExitMsg(PsychError_internal, "Assignment of capture callback fcn. to capture device failed!");            
    }
    
    // Store a reference to our slotid for this channel. This gets passed to the
    // videocapture callback fcn. so it knows to which capture object to relate to...
    error = SGSetChannelRefCon(vidcapRecordBANK[slotid].sgchanVideo, slotid);
    if (error !=noErr) {
        DisposeGWorld(vidcapRecordBANK[slotid].gworld);
        vidcapRecordBANK[slotid].gworld = NULL;
        SGDisposeChannel(seqGrab, *sgchanptr);
        *sgchanptr = NULL;
        CloseComponent(seqGrab);
        PsychErrorExitMsg(PsychError_internal, "Assignment of Refcon to capture device failed!");            
    }
    
    error = SGSetDataOutput(seqGrab, NULL, seqGrabDontMakeMovie);
    if (error !=noErr) {
        DisposeGWorld(vidcapRecordBANK[slotid].gworld);
        vidcapRecordBANK[slotid].gworld = NULL;
        SGDisposeChannel(seqGrab, *sgchanptr);
        *sgchanptr = NULL;
        CloseComponent(seqGrab);
        PsychErrorExitMsg(PsychError_internal, "Assignment of SGSetDataOutput() to capture device failed!");            
    }

    // Get ready!
    error = SGPrepare(seqGrab, false, true);
    if (error !=noErr) {
        DisposeGWorld(vidcapRecordBANK[slotid].gworld);
        vidcapRecordBANK[slotid].gworld = NULL;
        SGDisposeChannel(seqGrab, *sgchanptr);
        *sgchanptr = NULL;
        CloseComponent(seqGrab);
        PsychErrorExitMsg(PsychError_internal, "SGPrepare() for capture device failed!");            
    }
    
    // Grabber should be ready now.
    
    // Assign new record:
    vidcapRecordBANK[slotid].seqGrab=seqGrab;    

    // Assign final handle:
    *capturehandle = slotid;

    // Increase counter:
    numCaptureRecords++;

    // Query capture framerate: MK This doesn't return meaningful results for
    // some reason :(
    error=SGGetFrameRate(vidcapRecordBANK[slotid].sgchanVideo, &framerate);
    if (error==noErr) {
      // Query worked: Assign it.
	   vidcapRecordBANK[slotid].fps = (double) FixedToFloat(framerate);
	 }
    else {
      // Query failed: Assign a dummy value of 25 Hz and output a warning.
      vidcapRecordBANK[slotid].fps = 25;
      printf("PTB-WARNING: Couldn't determine real capture framerate of grabber device %i. Assigning dummy value of 25 fps.\n", slotid);
    }

    // Determine size of images in movie:
    vidcapRecordBANK[slotid].width = movierect.right - movierect.left;
    vidcapRecordBANK[slotid].height = movierect.bottom - movierect.top;
    
    // Reset framecounter:
    vidcapRecordBANK[slotid].nrframes = 0;

    printf("W x h = %i x  %i at %lf fps...\n", vidcapRecordBANK[slotid].width, vidcapRecordBANK[slotid].height, vidcapRecordBANK[slotid].fps);

    return(TRUE);
}

/*
 *  PsychCloseVideoCaptureDevice() -- Close a capture device and release all associated ressources.
 */
void PsychCloseVideoCaptureDevice(int capturehandle)
{
    if (capturehandle < 0 || capturehandle >= PSYCH_MAX_CAPTUREDEVICES) {
        PsychErrorExitMsg(PsychError_user, "Invalid capturehandle provided!");
    }
    
    if (vidcapRecordBANK[capturehandle].gworld == NULL) {
        PsychErrorExitMsg(PsychError_user, "Invalid capturehandle provided. No capture device associated with this handle !!!");
    }
        
    // Stop capture immediately:
    SGStop(vidcapRecordBANK[capturehandle].seqGrab);
    
    // Delete GWorld if any:
    if (vidcapRecordBANK[capturehandle].gworld) DisposeGWorld(vidcapRecordBANK[capturehandle].gworld);
    vidcapRecordBANK[capturehandle].gworld = NULL;
    
    // Release grabber:
    CloseComponent(vidcapRecordBANK[capturehandle].seqGrab);
    vidcapRecordBANK[capturehandle].seqGrab = NULL;
    vidcapRecordBANK[capturehandle].grabber_active = 0;
    
    // Decrease counter:
    if (numCaptureRecords>0) numCaptureRecords--;
        
    return;
}

/*
 *  PsychDeleteAllCaptureDevices() -- Delete all capture objects and release all associated ressources.
 */
void PsychDeleteAllCaptureDevices(void)
{
    int i;
    for (i=0; i<PSYCH_MAX_CAPTUREDEVICES; i++) {
        if (vidcapRecordBANK[i].gworld) PsychCloseVideoCaptureDevice(i);
    }
    return;
}


/*
 *  PsychGetTextureFromCapture() -- Create an OpenGL texturemap from a specific videoframe from given capture object.
 *
 *  win = Window pointer of onscreen window for which a OpenGL texture should be created.
 *  capturehandle = Handle to the capture object.
 *  checkForImage = true == Just check if new image available, false == really retrieve the image, blocking if necessary.
 *  timeindex = When not in playback mode, this allows specification of a requested frame by presentation time.
 *              If set to -1, or if in realtime playback mode, this parameter is ignored and the next video frame is returned.
 *  out_texture = Pointer to the Psychtoolbox texture-record where the new texture should be stored.
 *  presentation_timestamp = A ptr to a double variable, where the presentation timestamp of the returned frame should be stored.
 *  summed_intensity = An optional ptr to a double variable. If non-NULL, then sum of intensities over all channels is calculated and returned.
 *  Returns true (1) on success, false (0) if no new image available, -1 if no new image available and there won't be any in future.
 */
int PsychGetTextureFromCapture(PsychWindowRecordType *win, int capturehandle, int checkForImage, double timeindex, PsychWindowRecordType *out_texture, double *presentation_timestamp, double* summed_intensity)
{
    OSErr		error = noErr;
    GLuint texid;
    int w, h, padding;
    double targetdelta, realdelta, frames;
    unsigned int intensity = 0;
    unsigned int count, i;
    unsigned char* pixptr;
    Boolean newframe = FALSE;
    double tstart, tend;
    unsigned int pixval, alphacount;

    PsychGetAdjustedPrecisionTimerSeconds(&tstart);
    
    // Activate OpenGL context of target window: We'll need it for texture fetch...
    PsychSetGLContext(win);
    
    // Sanity checks:
    if (capturehandle < 0 || capturehandle >= PSYCH_MAX_CAPTUREDEVICES || vidcapRecordBANK[capturehandle].gworld == NULL) {
        PsychErrorExitMsg(PsychError_user, "Invalid capturehandle provided.");
    }
    
    if ((timeindex!=-1) && (timeindex < 0 || timeindex >= 10000.0)) {
        PsychErrorExitMsg(PsychError_user, "Invalid timeindex provided.");
    }

    // Grant some processing time to the sequence grabber engine:
    if (SGIdle(vidcapRecordBANK[capturehandle].seqGrab)!=noErr) {
        PsychErrorExitMsg(PsychError_internal, "SGIdle() failed!!!");
    }
    
    // Check if a new captured frame is ready for retrieval...
    newframe = (Boolean) vidcapRecordBANK[capturehandle].frame_ready;
    // ...and clear out the ready flag immediately:
    vidcapRecordBANK[capturehandle].frame_ready = 0;

    // Presentation timestamp requested?
    if (presentation_timestamp) {
        // Already available? Return it:
        *presentation_timestamp =vidcapRecordBANK[capturehandle].current_pts;
    }
    
    // Should we just check for new image? If so, just return availability status:
    if (checkForImage) {
        if (vidcapRecordBANK[capturehandle].grabber_active == 0) {
            // Grabber stopped. We'll never get a new image:
            return(-1);
        }

        // Timestamping:
        PsychGetAdjustedPrecisionTimerSeconds(&tend);
        vidcapRecordBANK[capturehandle].nrgfxframes++;
        vidcapRecordBANK[capturehandle].avg_gfxtime+=(tend - tstart);

        // Grabber active. Just return availability status:
        return(newframe);
    }
    
    // This point is only reached if checkForImage == FALSE, which only happens
    // if a new frame is available in our GWorld:

    // Synchronous texture fetch code for GWorld rendering mode:
        
    // Disable client storage on OS-X, if it is enabled for some reason:
    #if PSYCH_SYSTEM == PSYCH_OSX
      glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
    #endif
    
    // Build a standard PTB texture record:    
    
    // Assign texture rectangle:
    w=vidcapRecordBANK[capturehandle].width;
    h=vidcapRecordBANK[capturehandle].height;

    // Hack: Need to extend rect by 4 pixels, because GWorlds are 4 pixels-aligned via
    // image row padding:
#if PSYCH_SYSTEM == PSYCH_OSX
    padding = 4 + (4 - (w % 4)) % 4;
#else
    padding= 0;
#endif
    
    if (out_texture) {
        PsychMakeRect(out_texture->rect, 0, 0, w+padding, h);    
        
        // Set NULL - special texture object as part of the PTB texture record:
        out_texture->targetSpecific.QuickTimeGLTexture = NULL;
        
        // Set textureNumber to zero, which means "Not cached, don't recycle"
        // Todo: Texture recycling like in PsychMovieSupport for higher efficiency!
        out_texture->textureNumber = 0;
        
        // Set texture orientation as if it were an inverted Offscreen window: Upside-down.
        out_texture->textureOrientation = 3;
        
        // Setup a pointer to our GWorld as texture data pointer: Settin memsize to zero
        // prevents unwanted free() operation in PsychDeleteTexture...
        out_texture->textureMemorySizeBytes = 0;
    }
    
    // Lock GWorld:
    if(!LockPixels(GetGWorldPixMap(vidcapRecordBANK[capturehandle].gworld))) {
        // Locking surface failed! We abort.
        PsychErrorExitMsg(PsychError_internal, "PsychGetTextureFromCapture(): Locking GWorld pixmap surface failed!!!");
    }
    
    if (out_texture) {
        // This will retrieve an OpenGL compatible pointer to the GWorlds pixel data and assign it to our texmemptr:
        out_texture->textureMemory = (GLuint*) GetPixBaseAddr(GetGWorldPixMap(vidcapRecordBANK[capturehandle].gworld));
        
        // Let PsychCreateTexture() do the rest of the job of creating, setting up and
        // filling an OpenGL texture with GWorlds content:
        PsychCreateTexture(out_texture);

        // Undo hack from above after texture creation: Now we need the real width of the
        // texture for proper texture coordinate assignments in drawing code et al.
        PsychMakeRect(out_texture->rect, 0, 0, w-padding, h);    
        // Ready to use the texture...
    }
    
    // Sum of pixel intensities requested?
    if(summed_intensity) {
        pixptr = (unsigned char*) GetPixBaseAddr(GetGWorldPixMap(vidcapRecordBANK[capturehandle].gworld));
        count = (w*h*4);
        alphacount = 0;
        for (i=0; i<count; i++) {
            pixval=(unsigned int) pixptr[i];
            // Is this the alpha-channel component which is fixed to 255? If so, count it.
            if (pixval==255) alphacount++;
            intensity+=pixval;
        }
        // Try to discount the w*h*255 alpha channel values, if alpha channel is fixed to 255:
        // Some video digitizers set alpha component correctly to 255, some leave it at the
        // false value of zero :(
        if (alphacount >= w*h) intensity = intensity - (w * h * 255);
        *summed_intensity = ((double) intensity) / w / h / 3;
    }

    // Unlock GWorld surface.
    UnlockPixels(GetGWorldPixMap(vidcapRecordBANK[capturehandle].gworld));
    
    // Detection of dropped frames: This is a heuristic. We'll see how well it works out...
    
    // Expected delta between successive presentation timestamps:
    targetdelta = 1.0f / vidcapRecordBANK[capturehandle].fps;
    
    // Compute real delta, given rate and playback direction:
    realdelta = *presentation_timestamp - vidcapRecordBANK[capturehandle].last_pts;
    if (realdelta<0) realdelta = 0;
    frames = realdelta / targetdelta;

    // Dropped frames?
    if (frames > 1 && vidcapRecordBANK[capturehandle].last_pts>=0) {
        vidcapRecordBANK[capturehandle].nr_droppedframes += (int) (frames - 1 + 0.5);
    }

	 // Record timestamp as reference for next check:    
    vidcapRecordBANK[capturehandle].last_pts = *presentation_timestamp;
    
    // Timestamping:
    PsychGetAdjustedPrecisionTimerSeconds(&tend);
    vidcapRecordBANK[capturehandle].nrgfxframes++;
    vidcapRecordBANK[capturehandle].avg_gfxtime+=(tend - tstart);
    
    // We're successfully done!
    return(TRUE);
}

/*
 *  PsychVideoCaptureRate() - Start- and stop video capture.
 *
 *  capturehandle = Grabber to start-/stop.
 *  playbackrate = zero == Stop capture, non-zero == Capture
 *  dropframes = Currently ignored.
 *  Returns Number of dropped frames during capture.
 */
int PsychVideoCaptureRate(int capturehandle, double capturerate, int dropframes)
{
    int dropped = 0;
    OSErr error = noErr;
    Fixed framerate;
    
    if (capturehandle < 0 || capturehandle >= PSYCH_MAX_CAPTUREDEVICES) {
        PsychErrorExitMsg(PsychError_user, "Invalid capturehandle provided!");
    }
        
    // Fetch references to objects we need:
    if (vidcapRecordBANK[capturehandle].seqGrab == NULL) {
        PsychErrorExitMsg(PsychError_user, "Invalid capturehandle provided. No movie associated with this handle !!!");
    }
    
    if (capturerate > 0) {
        // Start capture:
        if (vidcapRecordBANK[capturehandle].grabber_active) PsychErrorExitMsg(PsychError_user, "You tried to start video capture, but capture is already started!");

        error = SGStartRecord(vidcapRecordBANK[capturehandle].seqGrab);
        vidcapRecordBANK[capturehandle].last_pts = -1.0;
        vidcapRecordBANK[capturehandle].nr_droppedframes = 0;
        vidcapRecordBANK[capturehandle].frame_ready = 0;
        vidcapRecordBANK[capturehandle].grabber_active = 1;
        framerate = FloatToFixed((float) capturerate);
        SGSetFrameRate(vidcapRecordBANK[capturehandle].sgchanVideo, framerate);
        SGGetFrameRate(vidcapRecordBANK[capturehandle].sgchanVideo, &framerate);
        vidcapRecordBANK[capturehandle].fps = (double) FixedToFloat(framerate);
        printf("FRAMERATE: %f\n", vidcapRecordBANK[capturehandle].fps);
    }
    else {
        // Stop capture:
        error = SGStop(vidcapRecordBANK[capturehandle].seqGrab);
        vidcapRecordBANK[capturehandle].frame_ready = 0;
        vidcapRecordBANK[capturehandle].grabber_active = 0;

        // Output count of dropped frames:
        if ((dropped=vidcapRecordBANK[capturehandle].nr_droppedframes) > 0) {
            printf("PTB-INFO: Video capture dropped %i frames on device %i to keep pipe running.\n", vidcapRecordBANK[capturehandle].nr_droppedframes, capturehandle); 
        }
        if (vidcapRecordBANK[capturehandle].nrframes>0)  vidcapRecordBANK[capturehandle].avg_decompresstime/= (double)vidcapRecordBANK[capturehandle].nrframes;
        printf("PTB-INFO: Average time spent in video decompressor was %lf milliseconds.\n", vidcapRecordBANK[capturehandle].avg_decompresstime * 1000.0f);
        if (vidcapRecordBANK[capturehandle].nrgfxframes>0)  vidcapRecordBANK[capturehandle].avg_gfxtime/= (double)vidcapRecordBANK[capturehandle].nrgfxframes;
        printf("PTB-INFO: Average time spent in GetCapturedImage was %lf milliseconds.\n", vidcapRecordBANK[capturehandle].avg_gfxtime * 1000.0f);
    }
    
    // Reset framecounter:
    vidcapRecordBANK[capturehandle].nrframes = 0;
    vidcapRecordBANK[capturehandle].avg_decompresstime = 0;
    vidcapRecordBANK[capturehandle].nrgfxframes = 0;
    vidcapRecordBANK[capturehandle].avg_gfxtime = 0;
    
    // Return either real capture framerate (at start of capture) or count of dropped frames - at end of capture.
    return((capturerate!=0) ? (int) (vidcapRecordBANK[capturehandle].fps + 0.5) : dropped);
}

/* Set capture device specific parameters:
 * On OS-X and Windows (and therefore in this implementation) this is currently a no-op, until
 * we find out how to do this with the Sequence-Grabber API.
 */
double PsychVideoCaptureSetParameter(int capturehandle, const char* pname, double value)
{
  // Just return the "not supported" value DBL_MAX:
  return(DBL_MAX);
}

/*
 *  void PsychExitVideoCapture() - Shutdown handler.
 *
 *  This routine is called by Screen('CloseAll') and on clear Screen time to
 *  do final cleanup. It deletes all capture objects
 *
 */
void PsychExitVideoCapture(void)
{
    // Release all capture devices
    PsychDeleteAllCaptureDevices();
    
    // Shutdown Quicktime toolbox: We skip this, because according to Apple its not necessary,
    // and for some reason it reliably hangs Matlab, so one has to force-quit it :-(
    // Don't do this: ExitMovies();
#if PSYCH_SYSTEM == PSYCH_WINDOWS
    // Shutdown Quicktime core system:
    ExitMovies();
    
    // Shutdown Quicktime for Windows compatibility layer:
    TerminateQTML();
#endif

    firsttime = TRUE;
    return;
}

#endif
