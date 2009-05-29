/*
	PsychToolbox3/Source/Common/Screen/PsychWindowSupport.c
	
	PLATFORMS:	
	
		All.  
				
	AUTHORS:
	
		Allen Ingling		awi	Allen.Ingling@nyu.edu
		Mario Kleiner		mk	mario.kleiner at tuebingen.mpg.de

	HISTORY:
	
		12/20/02		awi	Wrote it mostly by modifying SDL-specific refugees (from an experimental SDL-based Psychtoolbox).
		11/16/04		awi	Added description.
		4/22/05			mk      Added support for OpenGL stereo windows and enhanced Flip-behaviour:
						Flip'ing at specified deadline, retaining instead of clear'ing backbuffer during flip,
						return of stimulus onset related timestamps, properly syncing to VBL.
		4/29/05			mk      Additional safety checks for VBL sync in PsychOpenOnscreenWindow().
		5/14/05			mk      Additional safety checks for insufficient gfx-hardware and multi-display setups,
						failing beam-position queries. New refresh interval estimation code, reworked Flip.
		5/19/05			mk      Extra check for 'flipwhen' values over 1000 secs in future: Abort.
		5/30/05			mk      Check for Screen('Preference', 'SkipSyncTests', 1) -> Shortened tests, if set.
		6/09/05			mk      Experimental support for busy-waiting for VBL and for multi-flip of stereo displays.
		9/30/05			mk      Added PsychRealtimePriority for improving timing tests in PsychOpenWindow()
		9/30/05			mk      Added check for Screen('Preference', 'VisualDebugLevel', level) -> Amount of vis. feedback.
		10/10/05		mk      Important Bugfix for PsychRealtimePriority() - didn't switch back to non-RT priority!!
		10/19/05		awi	Cast NULL to CGLPixelFormatAttribute type to make the compiler happy.
		12/27/05		mk	PsychWindowSupport.h/c contains the shared parts of the windows implementation for all OS'es.
		3/07/06			awi	Print warnings conditionally according to PsychPrefStateGet_SuppressAllWarnings().
		11/14/06                mk      Replace blue screen by welcome text splash screen. Tighten threshold for frame skip detector for
		                                systems without beampos queries from 1.5 to 1.2, remove 0.5 offset and use unified deadline calculation
						code for the flipwhen>0 case and the flipwhen==0 case. All this should not matter on systems with beampos
						queries, but it makes the test more sensitive on systems without beampos queries, biasing it to more false
						positives on noisy systems, reducing the chance for false negatives.
                11/15/06                mk      Experimental support for low-level queries of vbl count and time from the driver: Used for verifying
                                                beampos query timestamping and as a fallback on systems that lack beampos query support.
 
	DESCRIPTION:
	
	NOTES:
	
        Documentation on the kernel-level shared memory access to the gfx-driver can be found here:
 
        http://developer.apple.com/documentation/Darwin/Reference/IOKit/IOFramebufferShared/index.html
	
        TO DO: 
	
*/

#include "Screen.h"

#if PSYCH_SYSTEM == PSYCH_LINUX
#include <errno.h>
#endif

#if PSYCH_SYSTEM != PSYCH_WINDOWS
#include "ptbstartlogo.h"
#else
/* This is a placeholder for ptbstartlogo.h until the fu%&$ing M$-Compiler can handle it.
 * GIMP RGBA C-Source image dump (welcomeWait.c)
 */
static const struct {
  unsigned int 	 width;
  unsigned int 	 height;
  unsigned int 	 bytes_per_pixel; /* 3:RGB, 4:RGBA */ 
  unsigned char	 pixel_data[4 + 1];
} gimp_image = {
  1, 1, 4, "    ",};
#endif

/* Flag which defines if userspace rendering is active: */
static boolean inGLUserspace = FALSE;

// We keep track of the current active rendertarget in order to
// avoid needless state changes:
static PsychWindowRecordType* currentRendertarget = NULL;

#if PSYCH_SYSTEM != PSYCH_WINDOWS
// The handle of the masterthread - The Matlab/Octave/PTB main interpreter thread: This
// is initialized when opening the first onscreen window. Its used in PsychSetDrawingTarget()
// to discriminate between the masterthread and the worker threads for async flip operations:
static pthread_t	masterthread = NULL;
#endif

// Count of currently async-flipping onscreen windows:
static unsigned int	asyncFlipOpsActive = 0;

// Dynamic rebinding of ARB extensions to core routines:
// This is a trick to get GLSL working on current OS-X (10.4.4). MacOS-X supports the OpenGL
// shading language on all graphics cards as an ARB extension. But as OS-X only supports
// OpenGL versions < 2.0 as of now, the functionality is not available as core functions, but
// only as their ARB counterparts. e.g., glCreateProgram() is always a NULL-Ptr on OS-X, but
// glCreateProgramObjectARB() is supported with exactly the same syntax and behaviour. By
// binding glCreateProgram as glCreateProgramObjectARB, we allow users to write Matlab code
// that uses glCreateProgram -- which is cleaner code than using glCreateProgramObjectARB,
// and it saves us from parsing tons of additional redundant function definitions anc code
// generation...
// In this function, we try to detect such OS dependent quirks and try to work around them...
void PsychRebindARBExtensionsToCore(void)
{   
    // Remap unsupported OpenGL 2.0 core functions for GLSL to supported ARB extension counterparts:
    if (NULL == glCreateProgram) glCreateProgram = glCreateProgramObjectARB;
    if (NULL == glCreateShader) glCreateShader = glCreateShaderObjectARB;
    if (NULL == glShaderSource) glShaderSource = glShaderSourceARB;
    if (NULL == glCompileShader) glCompileShader = glCompileShaderARB;
    if (NULL == glAttachShader) glAttachShader = glAttachObjectARB;
    if (NULL == glLinkProgram) glLinkProgram = glLinkProgramARB;
    if (NULL == glUseProgram) glUseProgram = glUseProgramObjectARB;
    if (NULL == glGetAttribLocation) glGetAttribLocation = glGetAttribLocationARB;
    // if (NULL == glGetUniformLocation) glGetUniformLocation = (GLint (*)(GLint, const GLchar*)) glGetUniformLocationARB;
    if (NULL == glGetUniformLocation) glGetUniformLocation = glGetUniformLocationARB;
    if (NULL == glUniform1f) glUniform1f = glUniform1fARB;
    if (NULL == glUniform2f) glUniform2f = glUniform2fARB;
    if (NULL == glUniform3f) glUniform3f = glUniform3fARB;
    if (NULL == glUniform4f) glUniform4f = glUniform4fARB;
    if (NULL == glUniform1fv) glUniform1fv = glUniform1fvARB;
    if (NULL == glUniform2fv) glUniform2fv = glUniform2fvARB;
    if (NULL == glUniform3fv) glUniform3fv = glUniform3fvARB;
    if (NULL == glUniform4fv) glUniform4fv = glUniform4fvARB;
    if (NULL == glUniform1i) glUniform1i = glUniform1iARB;
    if (NULL == glUniform2i) glUniform2i = glUniform2iARB;
    if (NULL == glUniform3i) glUniform3i = glUniform3iARB;
    if (NULL == glUniform4i) glUniform4i = glUniform4iARB;
    if (NULL == glUniform1iv) glUniform1iv = glUniform1ivARB;
    if (NULL == glUniform2iv) glUniform2iv = glUniform2ivARB;
    if (NULL == glUniform3iv) glUniform3iv = glUniform3ivARB;
    if (NULL == glUniform4iv) glUniform4iv = glUniform4ivARB;
    if (NULL == glUniformMatrix2fv) glUniformMatrix2fv = glUniformMatrix2fvARB;
    if (NULL == glUniformMatrix3fv) glUniformMatrix3fv = glUniformMatrix3fvARB;
    if (NULL == glUniformMatrix4fv) glUniformMatrix4fv = glUniformMatrix4fvARB;
    if (NULL == glGetShaderiv) glGetShaderiv = glGetObjectParameterivARB;
    if (NULL == glGetProgramiv) glGetProgramiv = glGetObjectParameterivARB;
    if (NULL == glGetShaderInfoLog) glGetShaderInfoLog = glGetInfoLogARB;
    if (NULL == glGetProgramInfoLog) glGetProgramInfoLog = glGetInfoLogARB;
    if (NULL == glValidateProgram) glValidateProgram = glValidateProgramARB;
    
    // Misc other stuff to remap...
    if (NULL == glDrawRangeElements) glDrawRangeElements = glDrawRangeElementsEXT;
    return;
}


/*
    PsychOpenOnscreenWindow()
    
    This routine first calls the operating system dependent setup routine in PsychWindowGlue to open
    an onscreen - window and create, setup and attach an OpenGL rendering context to it.

    Then it does all the OS independent setup stuff, like sanity and timing checks, determining the
    real monitor refresh and flip interval and start/endline of VBL via measurements and so on...

    -The pixel format and the context are stored in the target specific field of the window recored.  Close
    should clean up by destroying both the pixel format and the context.
    
    -We mantain the context because it must be be made the current context by drawing functions to draw into 
    the specified window.
    
    -We maintain the pixel format object because there seems to be no way to retrieve that from the context.
    
    -To tell the caller to clean up PsychOpenOnscreenWindow returns FALSE if we fail to open the window. It 
    would be better to just issue an PsychErrorExit() and have that clean up everything allocated outside of
    PsychOpenOnscreenWindow().
    
    MK: The new option 'stereomode' allows selection of stereo display instead of mono display:
    0 (default) == Old behaviour -> Monoscopic rendering context.
    >0          == Stereo display, where the number defines the type of stereo algorithm to use.
    =1          == Use OpenGL built-in stereo by creating a context/window with left- and right backbuffer.
    =2          == Use compressed frame stereo: Put both views into one framebuffer, one in top half, other in lower half.
 
    MK: Calibration/Measurement code was added that estimates the monitor refresh interval and the number of
        the last scanline (End of vertical blank interval). This increases time for opening an onscreen window
        by up to multiple seconds on slow (60 Hz) displays, but allows reliable syncing to VBL and kind of WaitBlanking
        functionality in Screen('Flip')... Also lots of tests for proper working of VBL-Sync and other stuff have been added.
        Contains experimental support for flipping multiple displays synchronously, e.g., for dual display stereo setups.
 
*/
boolean PsychOpenOnscreenWindow(PsychScreenSettingsType *screenSettings, PsychWindowRecordType **windowRecord, int numBuffers, int stereomode, double* rect, int multiSample, PsychWindowRecordType* sharedContextWindow)
{
    PsychRectType dummyrect;
    double ifi_nominal=0;    
    double ifi_estimate = 0;
    int retry_count=0;    
    int numSamples=0;
    double stddev=0;
    double maxsecs;    
    int VBL_Endline = -1;
    long vbl_startline, dummy_width;
    int i, maxline, bp;
    double tsum=0;
    double tcount=0;
    double ifi_beamestimate = 0;

    CGDirectDisplayID				cgDisplayID;
    int attribcount=0;
    int ringTheBell=-1;
    long VRAMTotal=0;
    long TexmemTotal=0;
    bool multidisplay = FALSE;
    bool sync_trouble = false;
    bool sync_disaster = false;
    int  skip_synctests = PsychPrefStateGet_SkipSyncTests();
    int visual_debuglevel = PsychPrefStateGet_VisualDebugLevel();
    int conserveVRAM = PsychPrefStateGet_ConserveVRAM();
    int logo_x, logo_y;
    GLboolean	isFloatBuffer;
    GLint bpc;
	
    // OS-9 emulation? If so, then we only work in double-buffer mode:
    if (PsychPrefStateGet_EmulateOldPTB()) numBuffers = 2;

    // Child protection: We need 2 AUX buffers for compressed stereo.
    if ((conserveVRAM & kPsychDisableAUXBuffers) && (stereomode==kPsychCompressedTLBRStereo || stereomode==kPsychCompressedTRBLStereo)) {
        printf("ERROR! You tried to disable AUX buffers via Screen('Preference', 'ConserveVRAM')\n while trying to use compressed stereo, which needs AUX-Buffers!\n");
        return(FALSE);
    }
    
    //First allocate the window recored to store stuff into.  If we exit with an error PsychErrorExit() should
    //call PsychPurgeInvalidWindows which will clean up the window record. 
    PsychCreateWindowRecord(windowRecord);  		//this also fills the window index field.

	// Show our "splash-screen wannabe" startup message at opening of first onscreen window:
	// Also init the thread handle to our main thread here:
	if ((*windowRecord)->windowIndex == PSYCH_FIRST_WINDOW) {
		if(PsychPrefStateGet_Verbosity()>2) {
			printf("\n\nPTB-INFO: This is the OpenGL-Psychtoolbox for %s, version %i.%i.%i. (Build date: %s)\n", PSYCHTOOLBOX_OS_NAME, PsychGetMajorVersionNumber(), PsychGetMinorVersionNumber(), PsychGetPointVersionNumber(), PsychGetBuildDate());
			printf("PTB-INFO: Type 'PsychtoolboxVersion' for more detailed version information.\n"); 
			printf("PTB-INFO: Psychtoolbox is licensed to you under terms of the GNU General Public License (GPL). See file 'License.txt' in the\n");
			printf("PTB-INFO: Psychtoolbox root folder for a copy of the GPL license.\n\n");
			
		}
		
		if (PsychPrefStateGet_EmulateOldPTB() && PsychPrefStateGet_Verbosity()>1) {
			printf("PTB-INFO: Psychtoolbox is running in compatibility mode to old MacOS-9 PTB. This is an experimental feature with\n");
			printf("PTB-INFO: limited support and possibly significant bugs hidden in it! Use with great caution and avoid if you can!\n");
			printf("PTB-INFO: Currently implemented: Screen('OpenOffscreenWindow'), Screen('CopyWindow') and Screen('WaitBlanking')\n");
		}
		
		#if PSYCH_SYSTEM != PSYCH_WINDOWS
			masterthread = pthread_self();
		#endif

	}
	
    // Assign the passed windowrect 'rect' to the new window:
    PsychCopyRect((*windowRecord)->rect, rect);
    
    // Assign requested level of multisampling for hardware Anti-Aliasing: 0 means - No hw-AA,
    // n>0 means: use hw-AA and try to get multisample buffers for at least n samples per pixel.
    // Todays hardware (as of mid 2006) typically supports 2x and 4x AA, Radeons support 6x AA.
    // If a request for n samples/pixel can't be satisfied by the hardware/OS, then we fall back
    // to the highest possible value. Worst case: We fall back to non-multisampled mode.
    // We pass in the requested value, after opening the window, the windowRecord contains
    // the real value used.
    (*windowRecord)->multiSample = multiSample;

    // Assign requested color buffer depth:
    (*windowRecord)->depth = screenSettings->depth.depths[0];
    
	// Explicit OpenGL context ressource sharing requested?
	if (sharedContextWindow) {
		// A pointer to a previously created onscreen window was provided and the OpenGL context of
		// the new window shall share ressources with the context of the provided window:
		(*windowRecord)->slaveWindow = sharedContextWindow;
	}
	
	// Automatic OpenGL context ressource sharing? By default, if no explicit sharing with
	// a specific sharedContextWindow is requested and context sharing is not disabled via
	// some 'ConserveVRAM' flag, we will try to share ressources of all OpenGL contexts
	// to simplify multi-window operations.
	if ((sharedContextWindow == NULL) && ((conserveVRAM & kPsychDontShareContextRessources) == 0) && (PsychCountOpenWindows(kPsychDoubleBufferOnscreen) + PsychCountOpenWindows(kPsychSingleBufferOnscreen) > 0)) {
		// Try context ressource sharing: Assign first onscreen window as sharing window:
		i = PSYCH_FIRST_WINDOW - 1;
		do {
			i++;
			FindWindowRecord(i, &((*windowRecord)->slaveWindow));
		} while (((*windowRecord)->slaveWindow->windowType != kPsychDoubleBufferOnscreen) && ((*windowRecord)->slaveWindow->windowType != kPsychSingleBufferOnscreen));
		// Ok, now we should have the first onscreen window assigned as slave window.
		if(PsychPrefStateGet_Verbosity()>3) printf("PTB-INFO: This oncreen window tries to share OpenGL context ressources with window %i.\n", i);
	}
	
    //if (PSYCH_DEBUG == PSYCH_ON) printf("Entering PsychOSOpenOnscreenWindow\n");
    
    // Call the OS specific low-level Window & Context setup routine:
    if (!PsychOSOpenOnscreenWindow(screenSettings, (*windowRecord), numBuffers, stereomode, conserveVRAM)) {
        printf("\nPTB-ERROR[Low-Level setup of window failed]:The specified display may not support double buffering and/or stereo output. There could be insufficient video memory\n\n");
        FreeWindowRecordFromPntr(*windowRecord);
        return(FALSE);
    }

	// At this point, the new onscreen windows master OpenGL context is active and bound...

	// Check for properly working glGetString() -- Some drivers (Some NVidia GF8/9 drivers on WinXP)
	// have a bug in conjunction with context ressource sharing here. Non-working glGetString is
	// a showstopper bug, but we should tell the user about the problem and stop safely instead
	// of taking whole runtime down:
	if (NULL == glGetString(GL_EXTENSIONS)) {
		// Game over:
		printf("PTB CRITICAL ERROR: Your graphics driver seems to have a bug which causes the OpenGL command glGetString() to malfunction!\n");
		printf("PTB CRITICAL ERROR: Can't continue safely, will therefore abort execution here.\n");
		printf("PTB CRITICAL ERROR: In the past this bug has been observed with some NVidia Geforce 8000 drivers under WindowsXP when using\n");
		printf("PTB CRITICAL ERROR: OpenGL 3D graphics mode. The recommended fix is to update your graphics drivers. A workaround that may\n");
		printf("PTB CRITICAL ERROR: work (but has its own share of problems) is to disable OpenGL context isolation. Type 'help ConserveVRAMSettings'\n");
		printf("PTB CRICICAL ERROR: and read the paragraph about setting '8' for more info.\n\n");

		// We abort! Close the onscreen window:
		PsychOSCloseWindow(*windowRecord);
		// Free the windowRecord:
		FreeWindowRecordFromPntr(*windowRecord);
		// Done. Return failure:
		return(FALSE);
	}

	#if PSYCH_SYSTEM == PSYCH_WINDOWS
    if(PsychPrefStateGet_Verbosity()>1) {
		if (strstr(glGetString(GL_RENDERER), "GDI")) {
			printf("\n\n\n\nPTB-WARNING: Seems that Microsofts OpenGL software renderer is active! This will likely cause miserable\n");
			printf("PTB-WARNING: performance, lack of functionality and severe timing and synchronization problems.\n");
			printf("PTB-WARNING: Most likely you do not have native OpenGL vendor supplied drivers (ICD's) for your graphics hardware\n");
			printf("PTB-WARNING: installed on your system.Many Windows machines (and especially Windows Vista) come without these preinstalled.\n");
			printf("PTB-WARNING: Go to the webpage of your computer vendor or directly to the webpage of NVidia/AMD/ATI/3DLabs/Intel\n");
			printf("PTB-WARNING: and make sure that you've download and install their latest driver for your graphics card.\n");
			printf("PTB-WARNING: Other causes, after you've ruled out the above:\n");
			printf("PTB-WARNING: Maybe you run at a too high display resolution, or the system is running out of ressources for some other reason.\n");
			printf("PTB-WARNING: Another reason could be that you disabled hardware acceleration in the display settings panel: Make sure that\n");
			printf("PTB-WARNING: in Display settings panel -> Settings -> Advanced -> Troubleshoot -> The hardware acceleration slider is\n");
			printf("PTB-WARNING: set to 'Full' (rightmost position).\n\n");
			printf("PTB-WARNING: Actually..., it is pointless to continue with the software renderer, that will cause more trouble than good.\n");
			printf("PTB-WARNING: I will abort now. Read the troubleshooting tips above to fix the problem. You can override this if you add the following\n");
			printf("PTB-WARNING: command: Screen('Preference', 'Verbosity', 1); to get a functional, but close to useless window up and running.\n\n\n");
			
			// We abort! Close the onscreen window:
			PsychOSCloseWindow(*windowRecord);

			// Free the windowRecord:
			FreeWindowRecordFromPntr(*windowRecord);

			// Done. Return failure:
			return(FALSE);			
		}
	}
	#endif

	// Set a flag that we should switch to native 10 bpc framebuffer later on if possible:
	if ((*windowRecord)->depth == 30) {
		// Support for kernel driver available?
#if PSYCH_SYSTEM == PSYCH_OSX || PSYCH_SYSTEM == PSYCH_LINUX
		if ((PSYCH_SYSTEM == PSYCH_LINUX) && (strstr(glGetString(GL_VENDOR), "NVIDIA") || (strstr(glGetString(GL_VENDOR), "ATI") && strstr(glGetString(GL_RENDERER), "Fire")))) {
			// NVidia GPU or ATI Fire-Series GPU: Only native support by driver, if at all...
			printf("\nPTB-INFO: Your script requested a 30bpp, 10bpc framebuffer, but this is only supported on few special graphics cards and drivers on MS-Windows.");
			printf("\nPTB-INFO: This may or may not work for you - Double check your results! Theoretically, the 2008 series ATI FireGL/FirePro and NVidia Quadro cards may support this with some drivers,");
			printf("\nPTB-INFO: but you must enable it manually in the Catalyst Control center (somewhere under ''Workstation settings'')\n");
		}
		else {
			if (!PsychOSIsKernelDriverAvailable(screenSettings->screenNumber)) {
				printf("\nPTB-ERROR: Your script requested a 30bpp, 10bpc framebuffer, but the Psychtoolbox kernel driver is not loaded and ready.\n");
				printf("PTB-ERROR: The driver currently only supports selected ATI Radeon GPU's (e.g., X1000/HD2000/HD3000/HD4000 series and later).\n");
				printf("PTB-ERROR: On MacOS/X the driver must be loaded and functional for your graphics card for this to work.\n");
				printf("PTB-ERROR: On Linux you must start Octave or Matlab as root, ie. system administrator or via sudo command for this to work.\n");
				printf("PTB-ERROR: Read 'help PsychtoolboxKernelDriver' for more information.\n\n");
				PsychOSCloseWindow(*windowRecord);
				FreeWindowRecordFromPntr(*windowRecord);
				return(FALSE);			
			}
			
			// Basic support seems to be there, set the request flag.
			(*windowRecord)->specialflags|= kPsychNative10bpcFBActive;
		}
#else
		// Not supported by our own code and kernel driver (we don't have such a driver for Windows), but some recent 2008
		// series FireGL cards at least provide the option to enable this natively - although it didn't work properly in our tests.
		printf("\nPTB-INFO: Your script requested a 30bpp, 10bpc framebuffer, but this is only supported on few special graphics cards and drivers on MS-Windows.");
		printf("\nPTB-INFO: This may or may not work for you - Double check your results! Theoretically, the 2008 series ATI FireGL/FirePro and NVidia Quadro cards may support this with some drivers,");
		printf("\nPTB-INFO: but you must enable it manually in the Catalyst Control center (somewhere under ''Workstation settings'')\n");
#endif
	}

	// Query if OpenGL stereo is supported:
	glGetBooleanv(GL_STEREO, &isFloatBuffer);
	if (!isFloatBuffer && stereomode==kPsychOpenGLStereo) {
		// OpenGL native stereo was requested, but is obviously not supported :-(
		// Let's abort here with some error message.
        printf("\nPTB-ERROR: Asked for OpenGL native stereo (frame-sequential mode) but this doesn't seem to be supported by your graphics hardware or driver.\n");
		if (PSYCH_SYSTEM == PSYCH_OSX) {
			printf("PTB-ERROR: Frame-sequential stereo should be supported on all recent ATI and NVidia cards on OS/X, except for the Intel onboard chips,\n");
			printf("PTB-ERROR: at least in fullscreen mode with OS/X 10.5, and also mostly on OS/X 10.4. If it doesn't work, check for OS updates etc.\n\n");
		}
		else {
			printf("PTB-ERROR: Frame-sequential stereo on Windows or Linux is usually only supported with the professional line of graphics cards\n");
			printf("PTB-ERROR: from NVidia and ATI/AMD, e.g., NVidia Quadro series or ATI FireGL series. Probably also with professional hardware from 3DLabs.\n");
			printf("PTB-ERROR: If you happen to have such a card, check your driver settings and/or update your graphics driver. This mode may also not\n");
			printf("PTB-ERROR: work on flat panels, due to their too low refresh rate.\n\n");
		}
		printf("PTB-ERROR: You may also check if your display resolution is too demanding, so your hardware runs out of VRAM memory.\n\n");

		PsychOSCloseWindow(*windowRecord);
        FreeWindowRecordFromPntr(*windowRecord);
        return(FALSE);		
	}

	if ((sharedContextWindow == NULL) && ((*windowRecord)->slaveWindow)) {
		// Undo slave window assignment from context sharing:
		(*windowRecord)->slaveWindow = NULL;
	}
	
    // Now we have a valid, visible onscreen (fullscreen) window with valid
    // OpenGL context attached. We mark it immediately as Onscreen window,
    // so in case of an error, the Screen('CloseAll') routine can properly
    // close it and release the Window system and OpenGL ressources.
    if(numBuffers==1) {
      (*windowRecord)->windowType=kPsychSingleBufferOnscreen;
    } 
    else {
      (*windowRecord)->windowType=kPsychDoubleBufferOnscreen;
    }

    // Dynamically rebind core extensions: Ugly ugly...
    PsychRebindARBExtensionsToCore();
    
    if ((((*windowRecord)->depth == 30) && !((*windowRecord)->specialflags & kPsychNative10bpcFBActive)) || (*windowRecord)->depth == 64 || (*windowRecord)->depth == 128) {

        // Floating point framebuffer active? GL_RGBA_FLOAT_MODE_ARB would be a viable alternative?
		isFloatBuffer = FALSE;
        glGetBooleanv(GL_COLOR_FLOAT_APPLE, &isFloatBuffer);
        if (isFloatBuffer) {
            printf("PTB-INFO: Floating point precision framebuffer enabled.\n");
        }
        else {
            printf("PTB-INFO: Fixed point precision integer framebuffer enabled.\n");
        }
        
        // Query and show bpc for all channels:
        glGetIntegerv(GL_RED_BITS, &bpc);
        printf("PTB-INFO: Frame buffer provides %i bits for red channel.\n", bpc);
        glGetIntegerv(GL_GREEN_BITS, &bpc);
        printf("PTB-INFO: Frame buffer provides %i bits for green channel.\n", bpc);
        glGetIntegerv(GL_BLUE_BITS, &bpc);
        printf("PTB-INFO: Frame buffer provides %i bits for blue channel.\n", bpc);
        glGetIntegerv(GL_ALPHA_BITS, &bpc);
        printf("PTB-INFO: Frame buffer provides %i bits for alpha channel.\n", bpc);
    }

	// Query if this onscreen window has a backbuffer with alpha channel, i.e.
	// it has more than zero alpha bits: 
	glGetIntegerv(GL_ALPHA_BITS, &bpc);
	
	// Windows are either RGB or RGBA, so either 3 or 4 channels. Here we
	// assign the default depths for this window record. This value needs to get
	// overriden when imaging pipeline is active, because there we use framebuffer
	// objects as backing store which always have RGBA 4 channel format.
	(*windowRecord)->nrchannels = (bpc > 0) ? 4 : 3;

	// We need the real color depth (bits per color component) of the framebuffer attached
	// to this onscreen window. We need it to setup color range correctly:
	if (!((*windowRecord)->specialflags & kPsychNative10bpcFBActive)) {
		// Let's assume the red bits value is representative for the green and blue channel as well:
		glGetIntegerv(GL_RED_BITS, &bpc);
	}
	else {
		// Special 10 bpc framebuffer activated by our own method:
		bpc = 10;
	}
	
	(*windowRecord)->colorRange = (double) ((1 << bpc) - 1);

    // Now we start to fill in the remaining windowRecord with settings:
    // -----------------------------------------------------------------

    // Normalize final windowRect: It is shifted so that its top-left corner is
    // always the origin (0,0). This way we lose the information about the absolute
    // position of the window on the screen, but this can be still queried from the
    // Screen('Rect') command for a screen index. Not normalizing creates breakage
    // in a lot of our own internal code, many demos and probably a lot of user code.
    PsychCopyRect(dummyrect, (*windowRecord)->rect);
    PsychNormalizeRect(dummyrect, (*windowRecord)->rect);

    // Compute logo_x and logo_y x,y offset for drawing the startup logo:
    logo_x = ((int) PsychGetWidthFromRect((*windowRecord)->rect) - (int) gimp_image.width) / 2;
    logo_x = (logo_x > 0) ? logo_x : 0;
    logo_y = ((int) PsychGetHeightFromRect((*windowRecord)->rect) - (int) gimp_image.height) / 2;
    logo_y = (logo_y > 0) ? logo_y : 0;

    //if (PSYCH_DEBUG == PSYCH_ON) printf("OSOpenOnscreenWindow done.\n");

    // Retrieve real number of samples/pixel for multisampling:
    (*windowRecord)->multiSample = 0;
    while(glGetError()!=GL_NO_ERROR);
    glGetIntegerv(GL_SAMPLES_ARB, (GLint*) &((*windowRecord)->multiSample));
    while(glGetError()!=GL_NO_ERROR);

    // Retrieve display handle for beamposition queries:
    PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, screenSettings->screenNumber);

    // Retrieve final vbl_startline, aka physical height of the display in pixels:
    PsychGetScreenSize(screenSettings->screenNumber, &dummy_width, &vbl_startline);
      
    // Associated screens id and depth:
    (*windowRecord)->screenNumber=screenSettings->screenNumber;
    (*windowRecord)->depth=PsychGetScreenDepthValue(screenSettings->screenNumber);
    
    // MK: Assign stereomode: 0 == monoscopic (default) window. >0 Stereo output window, where
    // the number specifies the type of stereo-algorithm used. Currently value 1 is
    // supported, which means: Output via OpenGL built-in stereo facilities. This can drive
    // all Stereo display devices that are supported by MacOS-X, e.g., the "Crystal Space"
    // Liquid crystal eye shutter glasses.
    // We also support value 2 and 3, which means "compressed" stereo: Only one framebuffer is used,
    // the left-eyes image is placed in the top half of the framebuffer, the right-eyes image is
    // placed int the bottom half of the buffer. One looses half of the vertical image resolution,
    // but both views are encoded in one video frame and can be decoded by external stereo-hardware,
    // e.g., the one available from CrystalEyes, this allows for potentially faster refresh.
    // Mode 4/5 is implemented by simple manipulations to the glViewPort...
    (*windowRecord)->stereomode = stereomode;
    
    // Setup timestamps and pipeline state for 'Flip' and 'DrawingFinished' commands of Screen:
    (*windowRecord)->time_at_last_vbl = 0;
    (*windowRecord)->PipelineFlushDone = false;
    (*windowRecord)->backBufferBackupDone = false;
    (*windowRecord)->nr_missed_deadlines = 0;
    (*windowRecord)->IFIRunningSum = 0;
    (*windowRecord)->nrIFISamples = 0;
    (*windowRecord)->VBL_Endline = -1;

    // Set the textureOrientation of onscreen windows to 2 aka "Normal, upright, non-transposed".
    // Textures of onscreen windows are created on demand as backup of the content of the onscreen
    // windows framebuffer. This happens in PsychSetDrawingTarget() if a switch from onscreen to
    // offscreen drawing target happens and the slow-path is used due to lack of Framebuffer-objects.
    // See code in PsychDrawingTarget()...
    (*windowRecord)->textureOrientation=2;

    // Perform a full safe reset of the framebuffer-object switching code:
    PsychSetDrawingTarget(0x1);
    
    // Enable this windowRecords OpenGL context and framebuffer as current drawingtarget. This will also setup
    // the projection and modelview matrices, viewports and such to proper values:
    PsychSetDrawingTarget(*windowRecord);

	if(PsychPrefStateGet_Verbosity()>2) {		
		  printf("\n\nOpenGL-Extensions are: %s\n\n", glGetString(GL_EXTENSIONS));
	}

	// Perform generic inquiry for interesting renderer capabilities and limitations/quirks
	// and setup the proper status bits for the windowRecord:
	PsychDetectAndAssignGfxCapabilities(*windowRecord);

#if PSYCH_SYSTEM == PSYCH_OSX
    CGLRendererInfoObj				rendererInfo;
    CGOpenGLDisplayMask 			displayMask;
    CGLError					error;

    displayMask=CGDisplayIDToOpenGLDisplayMask(cgDisplayID);

    if (true) {
        long numRenderers, i;
        error= CGLQueryRendererInfo(displayMask, &rendererInfo, &numRenderers);
        if(numRenderers>1) numRenderers=1;
        for(i=0;i<numRenderers;i++) {
            CGLDescribeRenderer(rendererInfo, i, kCGLRPVideoMemory, &VRAMTotal);
            CGLDescribeRenderer(rendererInfo, i, kCGLRPTextureMemory, &TexmemTotal);
        }
        CGLDestroyRendererInfo(rendererInfo);
    }

    // Are we running a multi-display setup? Then some tests and words of wisdom for the user are important
    // to reduce the traffic on the Psychtoolbox-Forum ;-)

    // Query number of physically connected and switched on displays...
    CGDisplayCount totaldisplaycount=0;
    CGGetOnlineDisplayList(0, NULL, &totaldisplaycount);
    
	// More than one display online?
	if (totaldisplaycount > 1) {
		// Yes. Is this an ATI GPU?
		if (strstr(glGetString(GL_VENDOR), "ATI")) {
			// Is this OS/X 10.5.7 or later?
			long osMinor, osBugfix, osArch;
			Gestalt(gestaltSystemVersionMinor, &osMinor);
			Gestalt(gestaltSystemVersionBugFix, &osBugfix);
			Gestalt(gestaltSysArchitecture, &osArch);
			
			if (osMinor == 5 && osBugfix >= 7 && osArch == gestaltIntel) {
				// OS/X 10.5.7 or later on IntelMac with an ATI GPU in dual-display or multi-display mode.
				// This specific configuration has serious bugs in CGDisplayBeamposition() beamposition
				// queries on multi-display setups. We mark the native beamposition mechanism as
				// unreliable, so our fallback kernel driver based solution is used instead - or
				// no beampos mechanism at all if driver not loaded:
				PsychPrefStateSet_ConserveVRAM(PsychPrefStateGet_ConserveVRAM() | kPsychDontUseNativeBeamposQuery);
				
				if(PsychPrefStateGet_Verbosity()>1) {
					printf("\n\nPTB-INFO: This is Mac OS/X 10.5.%i on an Intel Mac with an ATI GPU in multi-display mode.\n", (int) osBugfix);
					printf("PTB-INFO: Beamposition queries are broken on this configuration! Will disable them.\n");
					printf("PTB-INFO: Our own beamposition mechanism will still work though if you have the PsychtoolboxKernelDriver loaded.\n");
					printf("PTB-INFO: Type 'help PsychtoolboxKernelDriver' at the command prompt for more info about this option.\n\n");
				}
			}
		}
	}
	
    if(PsychPrefStateGet_Verbosity()>1){
		multidisplay = (totaldisplaycount>1) ? true : false;    
		if (multidisplay) {
			printf("\n\nPTB-INFO: You are using a multi-display setup (%i active displays):\n", totaldisplaycount);
			printf("PTB-INFO: Please read 'help MultiDisplaySetups' for specific information on the Do's, Dont's,\n");
			printf("PTB-INFO: and possible causes of trouble and how to diagnose and resolve them."); 
		}
		
		if (multidisplay && (!CGDisplayIsInMirrorSet(cgDisplayID) || PsychGetNumDisplays()>1)) {
			// This is a multi-display setup with separate (non-mirrored) displays: Bad for presentation timing :-(
			// Output some warning message to user, but continue. After all its the users
			// decision... ...and for some experiments were you need to show two different stims on two connected
			// monitors (haploscope, some stereo or binocular rivalry stuff) it is necessary. Let's hope they bought
			// a really fast gfx-card with plenty of VRAM :-)
			printf("\n\nPTB-INFO: According to the operating system, some of your connected displays do not seem to \n");
			printf("PTB-INFO: be switched into mirror mode. For a discussion of mirror mode vs. non-mirror mode,\n");
			printf("PTB-INFO: please read 'help MirrorMode'.\n");
		}
		
		if (CGDisplayIsInMirrorSet(cgDisplayID) && !CGDisplayIsInHWMirrorSet(cgDisplayID)) {
			// This is a multi-display setup with software-mirrored displays instead of hardware-mirrored ones: Not so good :-(
			// Output some warning message to user, but continue. After all its the users
			// decision...
			printf("\n\nPTB-WARNING: Seems that not all connected displays are switched into HARDWARE-mirror mode!\n");
			printf("PTB-WARNING: This could cause reduced drawing performance and inaccurate/wrong stimulus\n");
			printf("PTB-WARNING: presentation timing or skipped frames when showing moving/movie stimuli.\n");
			printf("PTB-WARNING: Seems that only SOFTWARE-mirroring is available for your current setup. You could\n");
			printf("PTB-WARNING: try to promote hardware-mirroring by trying different display settings...\n");
			printf("PTB-WARNING: If you still get this warning after putting your displays into mirror-mode, then\n");
			printf("PTB-WARNING: your system is unable to use hardware-mirroring and we recommend switching to a\n");
			printf("PTB-WARNING: single display setup if you encounter timing problems...\n\n");
			// Flash our visual warning bell:
			if (ringTheBell<1) ringTheBell=1;
		}
   } 
#endif

    // If we are in stereo mode 4 or 5 (free-fusion, cross-fusion, desktop-spanning stereo),
    // we need to enable Scissor tests to restrict drawing and buffer clear operations to
    // the currently set glScissor() rectangle (which is identical to the glViewport).
    if (stereomode == 4 || stereomode == 5) glEnable(GL_SCISSOR_TEST);


    if (numBuffers<2) {
		if(PsychPrefStateGet_Verbosity()>1){
			// Setup for single-buffer mode is finished!
			printf("\n\nPTB-WARNING: You are using a *single-buffered* window. This is *strongly discouraged* unless you\n");
			printf("PTB-WARNING: *really* know what you're doing! Stimulus presentation timing and all reported timestamps\n");
			printf("PTB-WARNING: will be inaccurate or wrong and synchronization to the vertical retrace will not work.\n");
			printf("PTB-WARNING: Please use *double-buffered* windows when doing anything else than debugging the PTB.\n\n");
			// Flash our visual warning bell:
			if (ringTheBell<2) ringTheBell=2;
			if (ringTheBell>=0) PsychVisualBell((*windowRecord), 4, ringTheBell);
			//mark the contents of the window record as valid.  Between the time it is created (always with PsychCreateWindowRecord) and when it is marked valid 
			//(with PsychSetWindowRecordValid) it is a potential victim of PsychPurgeInvalidWindows. 
		}
        PsychSetWindowRecordValid(*windowRecord);
        return(TRUE);
    }
    
    // Everything below this line is only for double-buffered contexts!

    // Activate syncing to onset of vertical retrace (VBL) for double-buffered windows:
    PsychOSSetVBLSyncLevel(*windowRecord, 1);
    
    // Setup of initial interframe-interval by multiple methods, for comparison:
    
    // First we query what the OS thinks is our monitor refresh interval:
    if (PsychGetNominalFramerate(screenSettings->screenNumber) > 0) {
        // Valid nominal framerate returned by OS: Calculate nominal IFI from it.
        ifi_nominal = 1.0 / ((double) PsychGetNominalFramerate(screenSettings->screenNumber));        
    }

    // This is pure eye-candy: We clear both framebuffers to a background color,
    // just to get rid of the junk that's in the framebuffers...
    // If visual debuglevel < 4 then we clear to black background...
    if (visual_debuglevel >= 4) {
      // Clear to white to prepare drawing of our logo:
      glClearColor(1,1,1,0);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }
    else {
      // Clear to black:
      glClearColor(0,0,0,0);
    }

    glDrawBuffer(GL_BACK_LEFT);
    glClear(GL_COLOR_BUFFER_BIT);

    glPixelZoom(1, -1);
    if (visual_debuglevel>=4) { glRasterPos2i(logo_x, logo_y); glDrawPixels(gimp_image.width, gimp_image.height, GL_RGBA, GL_UNSIGNED_BYTE, (void*) &gimp_image.pixel_data[0]); }
    PsychOSFlipWindowBuffers(*windowRecord);
    glClear(GL_COLOR_BUFFER_BIT);
    if (visual_debuglevel>=4) { glRasterPos2i(logo_x, logo_y); glDrawPixels(gimp_image.width, gimp_image.height, GL_RGBA, GL_UNSIGNED_BYTE, (void*) &gimp_image.pixel_data[0]); }
    PsychOSFlipWindowBuffers(*windowRecord);
    // We do it twice to clear possible stereo-contexts as well...
    if ((*windowRecord)->stereomode==kPsychOpenGLStereo) {
	glDrawBuffer(GL_BACK_RIGHT);
	glClear(GL_COLOR_BUFFER_BIT);
	if (visual_debuglevel>=4) { glRasterPos2i(logo_x, logo_y); glDrawPixels(gimp_image.width, gimp_image.height, GL_RGBA, GL_UNSIGNED_BYTE, (void*) &gimp_image.pixel_data[0]); }
	PsychOSFlipWindowBuffers(*windowRecord);
	glClear(GL_COLOR_BUFFER_BIT);
	if (visual_debuglevel>=4) { glRasterPos2i(logo_x, logo_y); glDrawPixels(gimp_image.width, gimp_image.height, GL_RGBA, GL_UNSIGNED_BYTE, (void*) &gimp_image.pixel_data[0]); }
	PsychOSFlipWindowBuffers(*windowRecord);
    }    
    glPixelZoom(1, 1);

    glDrawBuffer(GL_BACK);

    // Make sure that the gfx-pipeline has settled to a stable state...
    glFinish();
    
    // Complete skip of sync tests and all calibrations requested?
    // This should be only done if Psychtoolbox is not used as psychophysics
    // toolbox, but simply as a windowing/drawing toolkit for OpenGL in Matlab/Octave.
    if (skip_synctests<2) {
      // Normal calibration and at least some sync testing requested:

      // First we try if PsychGetDisplayBeamPosition works and try to estimate monitor refresh from it:
      
      // Check if a beamposition of 0 is returned at two points in time on OS-X:
      i = 0;
      if (((int) PsychGetDisplayBeamPosition(cgDisplayID, (*windowRecord)->screenNumber) == 0) && (PSYCH_SYSTEM == PSYCH_OSX)) {
        // Recheck after 2 ms on OS-X:
        PsychWaitIntervalSeconds(0.002);
        if ((int) PsychGetDisplayBeamPosition(cgDisplayID, (*windowRecord)->screenNumber) == 0) {
	  // A constant value of zero is reported on OS-X -> Beam position queries unsupported
	  // on this combo of gfx-driver and hardware :(
	  i=12345;
        }
      }
      
      // Check if a beamposition of -1 is returned: This would indicate that beamposition queries
      // are not available on this system: This always happens on Linux as that feature is unavailable.
      if ((-1 != ((int) PsychGetDisplayBeamPosition(cgDisplayID, (*windowRecord)->screenNumber))) && (i!=12345)) {
	// Switch to RT scheduling for timing tests:
	PsychRealtimePriority(true);
	
	// Code for estimating the final scanline of the vertical blank interval of display (needed by Screen('Flip')):
	
	// Check if PsychGetDisplayBeamPosition is working properly:
	// The first test checks, if it returns changing values at all or if it returns a constant
	// value at two measurements 2 ms apart...
	i=(int) PsychGetDisplayBeamPosition(cgDisplayID, (*windowRecord)->screenNumber);
	PsychWaitIntervalSeconds(0.002);
	if ((((int) PsychGetDisplayBeamPosition(cgDisplayID, (*windowRecord)->screenNumber)) == i) || (i < -1)) {
	  // PsychGetDisplayBeamPosition returns the same value at two different points in time?!?
	  // That's impossible on anything else than a high-precision 500 Hz display!
	  // --> PsychGetDisplayBeamPosition is not working correctly for some reason.
	  sync_trouble = true;
	  if(PsychPrefStateGet_Verbosity()>1) {
			if (i >=-1) printf("\nWARNING: Querying rasterbeam-position doesn't work on your setup! (Returns a constant value %i)\n", i);
			if (i < -1) printf("\nWARNING: Querying rasterbeam-position doesn't work on your setup! (Returns a negative value %i)\n", i);
	  }
	}
	else {
	  // PsychGetDisplayBeamPosition works: Use it to find VBL-Endline...
	  // Sample over 50 monitor refresh frames:
	  double told, tnew;
	  for (i=0; i<50; i++) {
	    // Take beam position samples from current monitor refresh interval:
	    maxline = -1;
	    // We spin-wait until retrace and record our highest measurement:
	    while ((bp=(int) PsychGetDisplayBeamPosition(cgDisplayID, (*windowRecord)->screenNumber)) >= maxline) maxline=bp;
	    // We also take timestamps for "yet another way" to measure monitor refresh interval...
	    PsychGetAdjustedPrecisionTimerSeconds(&tnew);
	    if (i>0) {
	      tsum+=(tnew - told);
	      tcount+=1;
	    }
	    told=tnew;
	    
		// Another (in)sanity check. Negative values immediately after retrace?
		if((int) PsychGetDisplayBeamPosition(cgDisplayID, (*windowRecord)->screenNumber) < 0) {
			// Driver bug! Abort this...
			VBL_Endline = -1;
			tnew = -1;
			
			if(PsychPrefStateGet_Verbosity()>1) printf("WARNING: Measured a negative beam position value after VBL onset!?! Broken display driver!!\n");

			// Break out of measurement loop:
			break;
		}
		
	    // Update global maximum with current sample:
	    if (maxline > VBL_Endline) VBL_Endline = maxline;
	  }
	  
	  // Setup reasonable timestamp for time of last vbl in emulation mode:
	  if (PsychPrefStateGet_EmulateOldPTB()) (*windowRecord)->time_at_last_vbl = tnew;
	}        
	
	// Switch to previous scheduling mode after timing tests:
	PsychRealtimePriority(false);
	
	// Override setting for VBL endline provided by usercode?
	if (PsychPrefStateGet_VBLEndlineOverride() >= 0) {
		// Yes. Assign it:
		if(PsychPrefStateGet_Verbosity()>1) {
			printf("PTB-WARNING: Usercode provided an override setting for the total height of the display in scanlines (aka VTOTAL)\n");
			printf("PTB-WARNING: via explicit use of the Screen('Preference', 'VBLEndlineOverride', ...); command.\n");
			printf("PTB-WARNING: Auto-detected old value was %i. New value from override which will be used for all timing: %i.\n", VBL_Endline, PsychPrefStateGet_VBLEndlineOverride());
			printf("PTB-WARNING: This is ok for working around graphics driver bugs, but make sure you don't apply this accidentally\n");
			printf("PTB-WARNING: without knowing what you're doing or why!\n\n");
		}
		
		VBL_Endline = PsychPrefStateGet_VBLEndlineOverride();
	}
	
	// Is the VBL endline >= VBL startline - 1, aka screen height?
	if ((VBL_Endline < (int) vbl_startline - 1) || (VBL_Endline > vbl_startline * 1.25)) {
	  // Completely bogus VBL_Endline detected! Warn the user and mark VBL_Endline
	  // as invalid so it doesn't get used anywhere:
	  sync_trouble = true;
	  ifi_beamestimate = 0;
	  if(PsychPrefStateGet_Verbosity()>1) {
	    printf("\nWARNING: Couldn't determine end-line of vertical blanking interval for your display! Trouble with beamposition queries?!?\n");
	    printf("\nWARNING: Detected end-line is %i, which is either lower or more than 25%% higher than vbl startline %i --> Out of sane range!\n", VBL_Endline, vbl_startline);
	  }
	}
	else {
	  // Compute ifi from beampos:
	  ifi_beamestimate = tsum / tcount;
	}
      }
      else {
	// We don't have beamposition queries on this system:
	ifi_beamestimate = 0;
	// Setup fake-timestamp for time of last vbl in emulation mode:
	if (PsychPrefStateGet_EmulateOldPTB()) PsychGetAdjustedPrecisionTimerSeconds(&((*windowRecord)->time_at_last_vbl));
      }

	  // End of beamposition measurements and validation.
	  
	  // We now perform an initial calibration using VBL-Syncing of OpenGL:
	  // We use 50 samples (50 monitor refresh intervals) and provide the ifi_nominal
	  // as a hint to the measurement routine to stabilize it:
      
      // We try 3 times a 5 seconds max., in case something goes wrong...
      while(ifi_estimate==0 && retry_count<3) {
		  numSamples=50;      // Require at least 50 *valid* samples...
		  stddev=0.00020;     // Require a std-deviation less than 200 microseconds..
		  maxsecs=(skip_synctests) ? 1 : 5;  // If skipping of sync-test is requested, we limit the calibration to 1 sec.
		  retry_count++;
		  ifi_estimate = PsychGetMonitorRefreshInterval(*windowRecord, &numSamples, &maxsecs, &stddev, ifi_nominal);
		  if((PsychPrefStateGet_Verbosity()>1) && (ifi_estimate==0 && retry_count<3)) {
			printf("\nWARNING: VBL Calibration run No. %i failed. Retrying...\n", retry_count);
		  }
      }

      // Compare ifi_estimate from VBL-Sync against beam estimate. If we are in OpenGL native
      // flip-frame stereo mode, a ifi_estimate approx. 2 times the beamestimate would be valid
      // and we would correct it down to half ifi_estimate. If multiSampling is enabled, it is also
      // possible that the gfx-hw is not capable of downsampling fast enough to do it every refresh
      // interval, so we could get an ifi_estimate which is twice the real refresh, which would be valid.
      (*windowRecord)->VideoRefreshInterval = ifi_estimate;
      if ((*windowRecord)->stereomode == kPsychOpenGLStereo || (*windowRecord)->multiSample > 0) {
        // Flip frame stereo or multiSampling enabled. Check for ifi_estimate = 2 * ifi_beamestimate:
        if ((ifi_beamestimate>0 && ifi_estimate >= 0.9 * 2 * ifi_beamestimate && ifi_estimate <= 1.1 * 2 * ifi_beamestimate) ||
	    (ifi_beamestimate==0 && ifi_nominal>0 && ifi_estimate >= 0.9 * 2 * ifi_nominal && ifi_estimate <= 1.1 * 2 * ifi_nominal)
	    ){
	  // This seems to be a valid result: Flip-interval is roughly twice the monitor refresh interval.
	  // We "force" ifi_estimate = 0.5 * ifi_estimate, so ifi_estimate roughly equals to ifi_nominal and
	  // ifi_beamestimate, in order to simplify all timing checks below. We also store this value as
	  // video refresh interval...
	  ifi_estimate = ifi_estimate * 0.5f;
	  (*windowRecord)->VideoRefreshInterval = ifi_estimate;
	  if(PsychPrefStateGet_Verbosity()>2){
	    if ((*windowRecord)->stereomode == kPsychOpenGLStereo) {
	      printf("\nPTB-INFO: The timing granularity of stimulus onset/offset via Screen('Flip') is twice as long\n");
	      printf("PTB-INFO: as the refresh interval of your monitor when using OpenGL flip-frame stereo on your setup.\n");
	      printf("PTB-INFO: Please keep this in mind, otherwise you'll be confused about your timing.\n");
	    }
	    if ((*windowRecord)->multiSample > 0) {
	      printf("\nPTB-INFO: The timing granularity of stimulus onset/offset via Screen('Flip') is twice as long\n");
	      printf("PTB-INFO: as the refresh interval of your monitor when using Anti-Aliasing at multiSample=%i on your setup.\n",
		     (*windowRecord)->multiSample);
	      printf("PTB-INFO: Please keep this in mind, otherwise you'll be confused about your timing.\n");
	    }
	  }
        }
      }
    } // End of display calibration part I of synctests.
    else {
      // Complete skip of calibration and synctests: Mark all calibrations as invalid:
      ifi_beamestimate = 0;
    }

	if(PsychPrefStateGet_Verbosity()>2) printf("\n\nPTB-INFO: OpenGL-Renderer is %s :: %s :: %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    if(PsychPrefStateGet_Verbosity()>2) {
      if (VRAMTotal>0) printf("PTB-INFO: Renderer has %li MB of VRAM and a maximum %li MB of texture memory.\n", VRAMTotal / 1024 / 1024, TexmemTotal / 1024 / 1024);
      printf("PTB-INFO: VBL startline = %i , VBL Endline = %i\n", (int) vbl_startline, VBL_Endline);
      if (ifi_beamestimate>0) {
          printf("PTB-INFO: Measured monitor refresh interval from beamposition = %f ms [%f Hz].\n", ifi_beamestimate * 1000, 1/ifi_beamestimate);
          if (PsychPrefStateGet_VBLTimestampingMode()==3 && PSYCH_SYSTEM == PSYCH_OSX) {
              printf("PTB-INFO: Will try to use kernel-level interrupts for accurate Flip time stamping.\n");
          }
          else {
              if (PsychPrefStateGet_VBLTimestampingMode()>=0) printf("PTB-INFO: Will use beamposition query for accurate Flip time stamping.\n");
              if (PsychPrefStateGet_VBLTimestampingMode()< 0) printf("PTB-INFO: Beamposition queries are supported, but disabled. Using basic timestamping as fallback: Timestamps returned by Screen('Flip') will be less robust and accurate.\n");
          }
      }
      else {
          if ((PsychPrefStateGet_VBLTimestampingMode()==1 || PsychPrefStateGet_VBLTimestampingMode()==3) && PSYCH_SYSTEM == PSYCH_OSX) {
              printf("PTB-INFO: Beamposition queries unsupported on this system. Will try to use kernel-level vbl interrupts as fallback.\n");
          }
          else {
              printf("PTB-INFO: Beamposition queries unsupported or defective on this system. Using basic timestamping as fallback: Timestamps returned by Screen('Flip') will be less robust and accurate.\n");
          }
      }
      printf("PTB-INFO: Measured monitor refresh interval from VBLsync = %f ms [%f Hz]. (%i valid samples taken, stddev=%f ms.)\n",
	     ifi_estimate * 1000, 1/ifi_estimate, numSamples, stddev*1000);
      if (ifi_nominal > 0) printf("PTB-INFO: Reported monitor refresh interval from operating system = %f ms [%f Hz].\n", ifi_nominal * 1000, 1/ifi_nominal);
      printf("PTB-INFO: Small deviations between reported values are normal and no reason to worry.\n");
      if ((*windowRecord)->stereomode==kPsychOpenGLStereo) printf("PTB-INFO: Stereo display via OpenGL built-in frame-sequential stereo enabled.\n");
      if ((*windowRecord)->stereomode==kPsychCompressedTLBRStereo) printf("PTB-INFO: Stereo display via vertical image compression enabled (Top=LeftEye, Bot.=RightEye).\n");
      if ((*windowRecord)->stereomode==kPsychCompressedTRBLStereo) printf("PTB-INFO: Stereo display via vertical image compression enabled (Top=RightEye, Bot.=LeftEye).\n");
      if ((*windowRecord)->stereomode==kPsychFreeFusionStereo) printf("PTB-INFO: Stereo for free fusion or dual-display desktop spanning enabled (2-in-1 stereo).\n");
      if ((*windowRecord)->stereomode==kPsychFreeCrossFusionStereo) printf("PTB-INFO: Stereo via free cross-fusion enabled (2-in-1 stereo).\n");
      if ((*windowRecord)->stereomode==kPsychAnaglyphRGStereo) printf("PTB-INFO: Stereo display via Anaglyph Red-Green stereo enabled.\n");
      if ((*windowRecord)->stereomode==kPsychAnaglyphGRStereo) printf("PTB-INFO: Stereo display via Anaglyph Green-Red stereo enabled.\n");
      if ((*windowRecord)->stereomode==kPsychAnaglyphRBStereo) printf("PTB-INFO: Stereo display via Anaglyph Red-Blue stereo enabled.\n");
      if ((*windowRecord)->stereomode==kPsychAnaglyphBRStereo) printf("PTB-INFO: Stereo display via Anaglyph Blue-Red stereo enabled.\n");
      if ((*windowRecord)->stereomode==kPsychDualWindowStereo) printf("PTB-INFO: Stereo display via dual window output with imaging pipeline enabled.\n");
      if ((PsychPrefStateGet_ConserveVRAM() & kPsychDontCacheTextures) && (strstr(glGetString(GL_EXTENSIONS), "GL_APPLE_client_storage")==NULL)) {
		// User wants us to use client storage, but client storage is unavailable :(
		printf("PTB-WARNING: You asked me for reducing VRAM consumption but for this, your graphics hardware would need\n");
		printf("PTB-WARNING: to support the GL_APPLE_client_storage extension, which it doesn't! Sorry... :(\n");
      }
      if (PsychPrefStateGet_3DGfx()) printf("PTB-INFO: Support for OpenGL 3D graphics rendering enabled: 24 bit depth-buffer and 8 bit stencil buffer attached.\n");
      if (multiSample>0) {
	if ((*windowRecord)->multiSample >= multiSample) {
	  printf("PTB-INFO: Anti-Aliasing with %i samples per pixel enabled.\n", (*windowRecord)->multiSample);
	}
	if ((*windowRecord)->multiSample < multiSample && (*windowRecord)->multiSample>0) {
	  printf("PTB-WARNING: Anti-Aliasing with %i samples per pixel enabled. Requested value of %i not supported by hardware.\n",
		 (*windowRecord)->multiSample, multiSample);
	}
	if ((*windowRecord)->multiSample <= 0) {
	  printf("PTB-WARNING: Could not enable Anti-Aliasing as requested. Your hardware does not support this feature!\n");
	}
      }
      else {
	// Multisampling enabled by external code, e.g., operating system override on M$-Windows?
	if ((*windowRecord)->multiSample > 0) {
	  // Report this, so user is aware of possible issues reg. performance and stimulus properties:
	  printf("PTB-WARNING: Anti-Aliasing with %i samples per pixel enabled, contrary to Psychtoolboxs request\n", (*windowRecord)->multiSample);                        
	  printf("PTB-WARNING: for non Anti-Aliased drawing! This will reduce drawing performance and will affect\n");                        
	  printf("PTB-WARNING: low-level properties of your visual stimuli! Check your display settings for a way\n");                        
	  printf("PTB-WARNING: to disable this behaviour if you don't like it. I will try to forcefully disable it now,\n");                        
	  printf("PTB-WARNING: but have no way to check if disabling it worked.\n");                        
	}
      }
    }
    
    // Final master-setup for multisampling:
    if (multiSample>0) {
      // Try to enable multisampling in software:
      while(glGetError()!=GL_NO_ERROR);
      glEnable(0x809D); // 0x809D == GL_MULTISAMPLE_ARB
      while(glGetError()!=GL_NO_ERROR);
      // Set sampling algorithm to the most high-quality one, even if it is
      // computationally more expensive: This will only work if the NVidia
      // GL_NV_multisample_filter_hint extension is supported...
      glHint(0x8534, GL_NICEST); // Set MULTISAMPLE_FILTER_HINT_NV (0x8534) to NICEST.
      while(glGetError()!=GL_NO_ERROR);
    }
    else {
      // Try to disable multisampling in software. That is the best we can do here:
      while(glGetError()!=GL_NO_ERROR);
      glDisable(0x809D);
      while(glGetError()!=GL_NO_ERROR);
    }
    
	// Master override: If context isolation is disabled then we use the PTB internal context...
	if ((conserveVRAM & kPsychDisableContextIsolation) && (PsychPrefStateGet_Verbosity()>1)) {
		printf("PTB-WARNING: You disabled OpenGL context isolation. This will increase the probability of cross-talk between\n");
		printf("PTB-WARNING: Psychtoolbox and Matlab-OpenGL code. Only use this switch to work around broken graphics drivers,\n");
		printf("PTB-WARNING: try if a driver update would be a more sane option.\n");
	}

    // Autodetect and setup type of texture extension to use for high-perf texture mapping:
    PsychDetectTextureTarget(*windowRecord);

    if (skip_synctests < 2) {
      // Reliable estimate? These are our minimum requirements...
      if (numSamples<50 || stddev>0.001) {
		  sync_disaster = true;
		  if(PsychPrefStateGet_Verbosity()>1)
			  printf("\nWARNING: Couldn't compute a reliable estimate of monitor refresh interval! Trouble with VBL syncing?!?\n");
      }
      
      // Check for mismatch between measured ifi from glFinish() VBLSync method and the value reported by the OS, if any:
      // This would indicate that we have massive trouble syncing to the VBL!
      if ((ifi_nominal > 0) && (ifi_estimate < 0.9 * ifi_nominal || ifi_estimate > 1.1 * ifi_nominal)) {
        if(PsychPrefStateGet_Verbosity()>1)
	  printf("\nWARNING: Mismatch between measured monitor refresh interval and interval reported by operating system.\nThis indicates massive problems with VBL sync.\n");    
        sync_disaster = true;
      }
    
      // Another check for proper VBL syncing: We only accept monitor refresh intervals between 25 Hz and 250 Hz.
      // Lower- / higher values probably indicate sync-trouble...
      if (ifi_estimate < 0.004 || ifi_estimate > 0.040) {
        if(PsychPrefStateGet_Verbosity()>1)
	  printf("\nWARNING: Measured monitor refresh interval indicates a display refresh of less than 25 Hz or more than 250 Hz?!?\nThis indicates massive problems with VBL sync.\n");    
        sync_disaster = true;        
      }
    } // End of synctests part II.
    
    // This is a "last resort" fallback: If user requests to *skip* all sync-tests and calibration routines
    // and we are unable to compute any ifi_estimate, we will fake one in order to be able to continue.
    // Either we use the nominal framerate provided by the operating system, or - if that's unavailable as well -
    // we assume a monitor refresh of 60 Hz, the typical value for flat-panels.
    if (ifi_estimate==0 && skip_synctests) {
      ifi_estimate = (ifi_nominal>0) ? ifi_nominal : (1.0/60.0);
      (*windowRecord)->nrIFISamples=1;
      (*windowRecord)->IFIRunningSum=ifi_estimate;
      (*windowRecord)->VideoRefreshInterval = ifi_estimate;
      if(PsychPrefStateGet_Verbosity()>1) {
	if (skip_synctests < 2) {
	  printf("\nPTB-WARNING: Unable to measure monitor refresh interval! Using a fake value of %f milliseconds.\n", ifi_estimate*1000);
	}
	else {
	  printf("PTB-INFO: All display tests and calibrations disabled. Assuming a refresh interval of %f Hz. Timing will be inaccurate!\n", 1.0/ifi_estimate);
	}
      }
    }
    
    if (sync_disaster) {
		// We fail! Continuing would be too dangerous without a working VBL sync. We don't
		// want to spoil somebodys study just because s(he) is relying on a non-working sync.
		if(PsychPrefStateGet_Verbosity()>0){		
			printf("\n\n");
			printf("----- ! PTB - ERROR: SYNCHRONIZATION FAILURE ! ----\n\n");
			printf("One or more internal checks (see Warnings above) indicate that synchronization\n");
			printf("of Psychtoolbox to the vertical retrace (VBL) is not working on your setup.\n\n");
			printf("This will seriously impair proper stimulus presentation and stimulus presentation timing!\n");
			printf("Please read 'help SyncTrouble' for information about how to solve or work-around the problem.\n");
			printf("You can force Psychtoolbox to continue, despite the severe problems, by adding the command\n");
			printf("Screen('Preference', 'SkipSyncTests', 1); at the top of your script, if you really know what you are doing.\n\n\n");
		}
		
		// Abort right here if sync tests are enabled:
		if (!skip_synctests) {
			// We abort! Close the onscreen window:
			PsychOSCloseWindow(*windowRecord);
			// Free the windowRecord:
			FreeWindowRecordFromPntr(*windowRecord);
			// Done. Return failure:
			return(FALSE);
		}
		
		// Flash our visual warning bell at alert-level for 1 second if skipping sync tests is requested:
		PsychVisualBell((*windowRecord), 1, 2);
    }
    
    // Ok, basic syncing to VBL via CGLFlushDrawable + glFinish seems to work and we have a valid
    // estimate of monitor refresh interval...
    
    // Check for mismatch between measured ifi from beamposition and from glFinish() VBLSync method.
    // This would indicate that the beam position is reported from a different display device
    // than the one we are VBL syncing to. -> Trouble!
    if ((ifi_beamestimate < 0.8 * ifi_estimate || ifi_beamestimate > 1.2 * ifi_estimate) && (ifi_beamestimate > 0)) {
        if(PsychPrefStateGet_Verbosity()>1)
	  printf("\nWARNING: Mismatch between measured monitor refresh intervals! This indicates problems with rasterbeam position queries.\n");    
        sync_trouble = true;
    }

    if (sync_trouble) {
        // Fail-Safe: Mark VBL-Endline as invalid, so a couple of mechanisms get disabled in Screen('Flip') aka PsychFlipWindowBuffers().
        VBL_Endline = -1;
		if(PsychPrefStateGet_Verbosity()>1){		
			printf("\n\n");
			printf("----- ! PTB - WARNING: SYNCHRONIZATION TROUBLE ! ----\n\n");
			printf("One or more internal checks (see Warnings above) indicate that\n");
			printf("queries of rasterbeam position are not properly working for your setup.\n\n");
			printf("Psychtoolbox will work around this by using a different timing algorithm, \n");
			printf("but it will cause Screen('Flip') to report less accurate/robust timestamps\n");
			printf("for stimulus timing.\n");
			printf("Read 'help BeampositionQueries' for more info and troubleshooting tips.\n");
			printf("\n\n");
			// Flash our visual warning bell:
			if (ringTheBell<2) ringTheBell=2;
		}
    }

    // Assign our best estimate of the scanline which marks end of vertical blanking interval:
    (*windowRecord)->VBL_Endline = VBL_Endline;
	// Store estimated video refresh cycle from beamposition method as well:
    (*windowRecord)->ifi_beamestimate = ifi_beamestimate;
    //mark the contents of the window record as valid.  Between the time it is created (always with PsychCreateWindowRecord) and when it is marked valid 
    //(with PsychSetWindowRecordValid) it is a potential victim of PsychPurgeInvalidWindows.  
    PsychSetWindowRecordValid(*windowRecord);

    // Ring the visual bell for one second if anything demands this:
    if (ringTheBell>=0 && !skip_synctests) PsychVisualBell((*windowRecord), 1, ringTheBell);

    if (PsychPrefStateGet_EmulateOldPTB()) {
        // Perform all drawing and reading in the front-buffer for OS-9 emulation mode:
        glReadBuffer(GL_FRONT);
        glDrawBuffer(GL_FRONT);
    }

	// Check if 10 bpc native framebuffer support is requested:
	if (((*windowRecord)->specialflags & kPsychNative10bpcFBActive) && PsychOSIsKernelDriverAvailable((*windowRecord)->screenNumber)) {
		// Try to switch framebuffer to native 10 bpc mode:
		PsychEnableNative10BitFramebuffer((*windowRecord), TRUE);
	}

    // Done.
    return(TRUE);
}


/*
    PsychOpenOffscreenWindow()
    
    Accept specifications for the offscreen window in the platform-neutral structures, convert to native OpenGL structures,
    create the texture, allocate a window record and record the window specifications and memory location there.
    TO DO:  We need to walk down the screen number and fill in the correct value for the benefit of TexturizeOffscreenWindow
*/
boolean PsychOpenOffscreenWindow(double *rect, int depth, PsychWindowRecordType **windowRecord)
{
    // This is a complete no-op as everything is implemented in SCREENOpenOffscreenWindow at the moment.
    return(TRUE);
    
    //    return(PsychOSOpenOffscreenWindow(rect, depth, windowRecord));
}


void PsychCloseWindow(PsychWindowRecordType *windowRecord)
{
    PsychWindowRecordType	**windowRecordArray;
    int                         i, numWindows; 
    
	// Extra child-protection to protect against half-initialized windowRecords...
	if (!windowRecord->isValid) {
		if (PsychPrefStateGet_Verbosity()>5) {
			printf("PTB-ERROR: Tried to destroy invalid windowRecord. Screw up in init sequence?!? Skipped.\n");
			fflush(NULL);
		}
		
		return;
	}
	
	// If our to-be-destroyed windowRecord is currently bound as drawing target,
	// e.g. as onscreen window or offscreen window, then we need to safe-reset
	// our drawing engine - Unbind its FBO (if any) and reset current target to
	// 'none'.
	if (PsychGetDrawingTarget() == windowRecord) PsychSetDrawingTarget(0x1);
	
    if(PsychIsOnscreenWindow(windowRecord)){
				// Call cleanup routine for the flipInfo record (and possible associated threads):
				// This must be first in order to not get caught in infinite loops if this is
				// a window close due to error-abort or other abort with async flips active:
				PsychReleaseFlipInfoStruct(windowRecord);

				// Check if 10 bpc native framebuffer support was supposed to be enabled:
				if ((windowRecord->specialflags & kPsychNative10bpcFBActive) && PsychOSIsKernelDriverAvailable(windowRecord->screenNumber)) {
					// Try to switch framebuffer back to standard 8 bpc mode. This will silently
					// do nothing if framebuffer wasn't in non-8bpc mode:
					PsychEnableNative10BitFramebuffer(windowRecord, FALSE);
				}

                // Free possible shadow textures:
                PsychFreeTextureForWindowRecord(windowRecord);        
                
                // Make sure that OpenGL pipeline is done & idle for this window:
                PsychSetGLContext(windowRecord);
				
				// Execute hook chain for OpenGL related shutdown:
				PsychPipelineExecuteHook(windowRecord, kPsychCloseWindowPreGLShutdown, NULL, NULL, FALSE, FALSE, NULL, NULL, NULL, NULL);

				// Sync and idle the pipeline:
                glFinish();
                
				// Shutdown only OpenGL related parts of imaging pipeline for this windowRecord, i.e.
				// do the shutdown work which still requires a fully functional OpenGL context and
				// hook-chains:
				PsychShutdownImagingPipeline(windowRecord, TRUE);
				
				// Sync and idle the pipeline again:
                glFinish();

                // We need to NULL-out all references to the - now destroyed - OpenGL context:
                PsychCreateVolatileWindowRecordPointerList(&numWindows, &windowRecordArray);
                for(i=0;i<numWindows;i++) {
                    if (windowRecordArray[i]->targetSpecific.contextObject == windowRecord->targetSpecific.contextObject &&
                        (windowRecordArray[i]->windowType==kPsychTexture || windowRecordArray[i]->windowType==kPsychProxyWindow)) {
                        windowRecordArray[i]->targetSpecific.contextObject = NULL;
						windowRecordArray[i]->targetSpecific.glusercontextObject = NULL;
                    }
                }
                PsychDestroyVolatileWindowRecordPointerList(windowRecordArray);

                // Disable rendering context:
                PsychOSUnsetGLContext(windowRecord);

				// Call OS specific low-level window close routine:
				PsychOSCloseWindow(windowRecord);

                windowRecord->targetSpecific.contextObject=NULL;

				// Execute hook chain for final non-OpenGL related shutdown:
				PsychPipelineExecuteHook(windowRecord, kPsychCloseWindowPostGLShutdown, NULL, NULL, FALSE, FALSE, NULL, NULL, NULL, NULL);
				
				// If this was the last onscreen window then we reset the currentRendertarget etc. to pre-Screen load time:
				if (PsychIsLastOnscreenWindow(windowRecord)) {
					currentRendertarget = NULL;
					asyncFlipOpsActive = 0;
				}
    }
    else if(windowRecord->windowType==kPsychTexture) {
                // Texture or Offscreen window - which is also just a form of texture.
				PsychFreeTextureForWindowRecord(windowRecord);

				// Shutdown only OpenGL related parts of imaging pipeline for this windowRecord, i.e.
				// do the shutdown work which still requires a fully functional OpenGL context and
				// hook-chains:
				PsychShutdownImagingPipeline(windowRecord, TRUE);
    }
    else if(windowRecord->windowType==kPsychProxyWindow) {
				// Proxy window object without associated OpenGL state or content.
				// Run shutdown sequence for imaging pipeline in case the proxy has bounce-buffer or
				// lookup table textures or FBO's attached:
				PsychShutdownImagingPipeline(windowRecord, TRUE);
    }
    else if(windowRecord->windowType==kPsychNoWindow) {
				// Partially initialized windowRecord, not yet associated to a real Window system
				// window or OpenGL rendering context. We skip this one - there's nothing to do.
				// Well almost... ...we output some warning, as something must have screwed up seriously if
				// we reached this point in control-flow...
				printf("PTB-ERROR: Something is screwed up seriously! Please read all warnings and error messages\n");
				printf("PTB-ERROR: above these lines very carefully to assess and fix the problem...\n");
				fflush(NULL);
				return;
    }
    else {
                // If we reach this point then we've really screwed up, e.g., internal memory corruption.
				PsychErrorExitMsg(PsychError_internal, "FATAL ERROR: Unrecognized window type. Memory corruption?!?");
    }
    
	// Output count of missed deadlines. Don't bother for 1 missed deadline -- that's an expected artifact of the measurement...
    if (PsychIsOnscreenWindow(windowRecord) && (windowRecord->nr_missed_deadlines>1)) {
		if(PsychPrefStateGet_Verbosity()>1) {
			printf("\n\nINFO: PTB's Screen('Flip') command seems to have missed the requested stimulus presentation deadline\n");
			printf("INFO: a total of %i times during this session.\n\n", windowRecord->nr_missed_deadlines);
			printf("INFO: This number is fairly accurate (and indicative of real timing problems in your own code or your system)\n");
			printf("INFO: if you provided requested stimulus onset times with the 'when' argument of Screen('Flip', window [, when]);\n");
			printf("INFO: If you called Screen('Flip', window); without the 'when' argument, this count is more of a ''mild'' indicator\n");
			printf("INFO: of timing behaviour than a hard reliable measurement. Large numbers may indicate problems and should at least\n");
			printf("INFO: deserve your closer attention. Cfe. 'help SyncTrouble', the FAQ section at www.psychtoolbox.org and the\n");
			printf("INFO: examples in the PDF presentation in PsychDocumentation/Psychtoolbox3-Slides.pdf for more info and timing tips.\n\n");
		}
    }
    
    if (PsychIsOnscreenWindow(windowRecord) && PsychPrefStateGet_SkipSyncTests()) {
        if(PsychPrefStateGet_Verbosity()>1){
			printf("\n\nWARNING: This session of your experiment was run by you with the setting Screen('Preference', 'SkipSyncTests', %i).\n",
			       (int) PsychPrefStateGet_SkipSyncTests());
			printf("WARNING: This means that some internal self-tests and calibrations were skipped. Your stimulus presentation timing\n");
			printf("WARNING: may have been wrong. This is fine for development and debugging of your experiment, but for running the real\n");
			printf("WARNING: study, please make sure to set Screen('Preference', 'SkipSyncTests', 0) for maximum accuracy and reliability.\n");
		}
    }

	// Shutdown non-OpenGL related parts of imaging pipeline for this windowRecord:
	PsychShutdownImagingPipeline(windowRecord, FALSE);

    PsychErrorExit(FreeWindowRecordFromPntr(windowRecord));
}


/*
    PsychFlushGL()
    
    Enforce rendering of all pending OpenGL drawing commands and wait for render completion.
    This routine is called at the end of each Screen drawing subfunction. A call to it signals
    the end of a single Matlab drawing command.
 
    -If this is an onscreen window in OS-9 emulation mode we call glFinish();

    -In all other cases we don't do anything because CGLFlushDrawable which is called by PsychFlipWindowBuffers()
    implicitley calls glFlush() before flipping the buffers. Apple warns of lowered perfomance if glFlush() is called 
    immediately before CGLFlushDrawable().
 
    Note that a glFinish() after each drawing command can significantly impair overall drawing performance and
    execution speed of Matlab Psychtoolbox scripts, because the parallelism between CPU and GPU breaks down completely
    and we can run out of DMA command buffers, effectively stalling the CPU!
 
    We need to do this to provide backward compatibility for old PTB code from OS-9 or Win PTB, where
    synchronization to VBL is done via WaitBlanking: After a WaitBlanking, all drawing commands must execute as soon
    as possible and a GetSecs - call after *any* drawing command must return a useful timestamp for stimulus onset
    time. We can only achieve a compatible semantic by using a glFinish() after each drawing op.
*/
void PsychFlushGL(PsychWindowRecordType *windowRecord)
{
    if(PsychIsOnscreenWindow(windowRecord) && PsychPrefStateGet_EmulateOldPTB()) glFinish();            
}

/* PsychReleaseFlipInfoStruct() -- Cleanup flipInfo struct
 *
 * This routine cleans up the flipInfo struct field of onscreen window records at
 * onscreen window close time (called from PsychCloseWindow() for onscreen windows).
 * It also performs all neccessary thread shutdown and release actions if a async
 * thread is associated with the windowRecord.
 *
 */
void PsychReleaseFlipInfoStruct(PsychWindowRecordType *windowRecord)
{
	PsychFlipInfoStruct* flipRequest = windowRecord->flipInfo;
	int rc;
	static unsigned int recursionlevel = 0;
	
	// Nothing to do for NULL structs:
	if (NULL == flipRequest) return;
	
	// Any async flips in progress?
	if (flipRequest->asyncstate != 0) {
		// Hmm, what to do?
		printf("PTB-WARNING: Asynchronous flip operation for window %p in progress while Screen('Close') or Screen('CloseAll') was called or\n", windowRecord);
		printf("PTB-WARNING: exiting from a Screen error! Will try to finalize it gracefully. This may hang, crash or go into an infinite loop...\n");
		fflush(NULL);
		
		// A value of 2 would mean its basically done, so nothing to do here.
		if (flipRequest->asyncstate == 1) {
			// If no recursion and flipper thread not in error state it might be safe to try a normal shutdown:
			if (recursionlevel == 0 && flipRequest->flipperState < 4) {
				// Operation in progress: Try to stop it the normal way...
				flipRequest->opmode = 2;
				recursionlevel++;
				PsychFlipWindowBuffersIndirect(windowRecord);
				recursionlevel--;
			}
			else {
				// We seem to be in an infinite error loop. Try to force asyncstate to zero
				// in the hope that we'll break out of the loop that way and hope for the best...
				printf("PTB-WARNING: Infinite loop detected. Trying to break out in a cruel way. This may hang, crash or go into another infinite loop...\n");
				fflush(NULL);
				flipRequest->asyncstate = 0;
			}
		}
	}
	
	// Decrement the asyncFlipOpsActive count:
	asyncFlipOpsActive--;

#if PSYCH_SYSTEM != PSYCH_WINDOWS
	// Any threads attached?
	if (flipRequest->flipperThread) {
		// Yes. Cancel and destroy / release it, also release all mutex locks:

		// Set opmode to "terminate please":
		flipRequest->opmode = -1;
		
		// Unlock the lock in case we hold it, so the thread can't block on it:
		if ((rc=pthread_mutex_unlock(&(flipRequest->performFlipLock))) && (rc!=EPERM)) {
			printf("PTB-DEBUG: In PsychReleaseFlipInfoStruct(): mutex_unlock in thread shutdown operation failed  [%s].\n", strerror(rc));
			printf("PTB-ERROR: This must not ever happen! PTB design bug or severe operating system or runtime environment malfunction!! Memory corruption?!?");
			// Anyway, just hope its not fatal for us...
		}
		
		// Signal the thread in case its waiting on the condition variable:
		if ((rc=pthread_cond_signal(&(flipRequest->flipperGoGoGo)))) {
			printf("PTB-ERROR: In PsychReleaseFlipInfoStruct(): pthread_cond_signal in thread shutdown operation failed  [%s].\n", strerror(rc));
			printf("PTB-ERROR: This must not ever happen! PTB design bug or severe operating system or runtime environment malfunction!! Memory corruption?!?");
			// Anyway, just hope its not fatal for us...
			// Try to cancel the thread in a more cruel manner. That's the best we can do.
			pthread_cancel(flipRequest->flipperThread);
		}

		// Wait for thread to stop and die:
		if (PsychPrefStateGet_Verbosity()>5) printf("PTB-DEBUG: Waiting (join()ing) for helper thread of window %p to finish up. If this doesn't happen quickly, you'll have to kill Matlab/Octave...\n", windowRecord);

		// If any error happened here, it wouldn't be a problem for us...
		pthread_join(flipRequest->flipperThread, NULL);
		
		// Ok, thread is dead. Mark it as such:
		flipRequest->flipperThread = NULL;
	}
	
	// Destroy the mutex:
	if ((rc=pthread_mutex_destroy(&(flipRequest->performFlipLock))) && (rc!=EINVAL)) {
		printf("PTB-WARNING: In PsychReleaseFlipInfoStruct: Could not destroy performFlipLock mutex lock [%s].\n", strerror(rc));
		printf("PTB-WARNING: This will cause ressource leakage. Maybe you should better exit and restart Matlab/Octave?");
	}
	
	// Destroy condition variable:
	if ((rc=pthread_cond_destroy(&(flipRequest->flipperGoGoGo))) && (rc!=EINVAL)) {
		printf("PTB-WARNING: In PsychReleaseFlipInfoStruct: Could not destroy flipperGoGoGo condition variable [%s].\n", strerror(rc));
		printf("PTB-WARNING: This will cause ressource leakage. Maybe you should better exit and restart Matlab/Octave?");
	}
#endif

	// Release struct:
	free(flipRequest);
	windowRecord->flipInfo = NULL;
	
	// Done.
	return;
}

/* PsychFlipperThreadMain() the "main()" routine of the asynchronous flip worker thread:
 *
 * This routine implements an infinite loop (well, infinite until cancellation at Screen('Close')
 * time etc.). The loop waits for a trigger signal from the PTB/Matlab/Octave main thread that
 * an async flip for the associated onscreen window is requested. Then it processes that request
 * by calling the underlying flip routine properly, returns all return values in the flipRequest
 * struct and goes to sleep again to wait for the next request.
 *
 * Each onscreen window has its own thread, but threads are created lazily at first invokation of
 * an async fip for a window, so most users will never ever have any of these beasts running.
 * The threads are destroyed at Screen('Close', window); Screen('CloseAl') or Screen errorhandling/
 * clear Screen / Matlab exit time.
 *
 * While an async flip is active for an onscreen window, the worker thread exclusively owns the
 * OpenGL context of that window and the main thread is prevented from executing any OpenGL related
 * commands. This is important because while many OpenGL contexts are allowed to be attached to
 * many threads in parallel, it's not allowed for one context to be attached to multiple threads!
 * We have lots of locking and protection in place to prevent such things.
 *
 * Its also important that no code from a worker thread is allowed to call back into Matlab, so
 * the imaging pipeline can not run from this thread: PsychPreflipOperations() is run fromt the main
 * thread in a fully synchronous manner, only after imaging pipe completion is control handed to the
 * worker thread. Error output or error handling from within code executed here may or may not be
 * safe in Matlab, so if one of these errors triggers, things may screw up in uncontrolled ways,
 * ending in a hang or crash of Matlab/Octave. We try to catch most common errors outside the
 * worker thread to minimize chance of this happening.
 *
 */
void* PsychFlipperThreadMain(void* windowRecordToCast)
{
#if PSYCH_SYSTEM != PSYCH_WINDOWS
	int rc;

	// Get a handle to our info structs: These pointers must not be NULL!!!
	PsychWindowRecordType*	windowRecord = (PsychWindowRecordType*) windowRecordToCast;
	PsychFlipInfoStruct*	flipRequest	 = windowRecord->flipInfo;
	
	// Try to lock, block until available if not available:
	if ((rc=pthread_mutex_lock(&(flipRequest->performFlipLock)))) {
		// This could potentially kill Matlab, as we're printing from outside the main interpreter thread.
		// Use fprintf() instead of the overloaded printf() (aka mexPrintf()) in the hope that we don't
		// wreak havoc -- maybe it goes to the system log, which should be safer...
		fprintf(stderr, "PTB-ERROR: In PsychFlipperThreadMain(): First mutex_lock in init failed  [%s].\n", strerror(rc));

		// Commit suicide with state "error, lock not held":
		flipRequest->flipperState = 5;
		return;
	}
	
	// Got the lock: Set our state as "initialized, ready & waiting":
	flipRequest->flipperState = 1;

	// Dispatch loop: Repeats infinitely, processing one flip request per loop iteration.
	// Well, not infinitely, but until we receive a shutdown request and terminate ourselves...
	while (TRUE) {
		// Unlock the lock and go to sleep, waiting on the condition variable for a start signal from
		// the master thread. This is an atomic operation, both unlock and sleep happen simultaneously.
		// After a wakeup due to signalling, the lock is automatically reacquired, so no need to mutex_lock
		// anymore. This is also a thread cancellation point...
		if ((rc=pthread_cond_wait(&(flipRequest->flipperGoGoGo), &(flipRequest->performFlipLock)))) {
			// Failed: Log it in a hopefully not too unsafe way:
			fprintf(stderr, "PTB-ERROR: In PsychFlipperThreadMain():  pthread_cond_wait() on flipperGoGoGo trigger failed  [%s].\n", strerror(rc));
			
			// Commit suicide with state "error, lock not held":
			flipRequest->flipperState = 5;
			return;
		}
		
		// Got woken up, work to do! We have to lock from auto-reqacquire in cond_wait:
		
		// Check if we are supposed to terminate:
		if (flipRequest->opmode == -1) {
			// We shall terminate: We are not waiting on the flipperGoGoGo variable.
			// We hold the mutex, so set us to state "terminating with lock held" and exit the loop:
			flipRequest->flipperState = 4;
			break;	
		}

		// Got the lock: Set our state to "executing - flip in progress":
		flipRequest->flipperState = 2;

		// fprintf(stdout, "WAITING UNTIL T = %lf\n", flipRequest->flipwhen); fflush(NULL);

		// Setup context etc. manually, as PsychSetDrawingTarget() is a no-op when called from
		// this thread:
		
		// Attach to context - It's detached in the main thread:
		PsychSetGLContext(windowRecord);
		
		// Setup view:
		PsychSetupView(windowRecord);

		// Nothing more to do, the system backbuffer is bound, no FBO's are set at this point.

		// Unpack struct and execute synchronous flip: Synchronous in our thread, asynchronous from Matlabs/Octaves perspective!
		flipRequest->vbl_timestamp = PsychFlipWindowBuffers(windowRecord, flipRequest->multiflip, flipRequest->vbl_synclevel, flipRequest->dont_clear, flipRequest->flipwhen, &(flipRequest->beamPosAtFlip), &(flipRequest->miss_estimate), &(flipRequest->time_at_flipend), &(flipRequest->time_at_onset));

		// Flip finished and struct filled with return arguments.
		// Set our state to 3 aka "flip operation finished, ready for new commands":
		flipRequest->flipperState = 3;
		
		// Detach our GL context, so main interpreter thread can use it again. This will also unbind any bound FBO's.
		// As there wasn't any drawing target bound throughout our execution, and the drawingtarget was reset to
		// NULL in main thread before our invocation, there's none bound now. --> The first Screen command in
		// the main thread will rebind and setup the context and drawingtarget properly:
		PsychOSUnsetGLContext(windowRecord);

		//fprintf(stdout, "DETACHED, CONDWAIT\n"); fflush(NULL);
		
		// Repeat the dispatch loop. That will atomically unlock the lock and set us asleep until we
		// get triggered again with more work to do:
	}
	
	// Need to unlock the mutex:
	if (flipRequest->flipperState == 4) {
		if ((rc=pthread_mutex_unlock(&(flipRequest->performFlipLock)))) {
			// This could potentially kill Matlab, as we're printing from outside the main interpreter thread.
			// Use fprintf() instead of the overloaded printf() (aka mexPrintf()) in the hope that we don't
			// wreak havoc -- maybe it goes to the system log, which should be safer...
			fprintf(stderr, "PTB-ERROR: In PsychFlipperThreadMain(): Last mutex_unlock in termination failed  [%s].\n", strerror(rc));

			// Commit suicide with state "error, lock not held":
			flipRequest->flipperState = 5;
			return;
		}
	}

#endif

	// Ok, we're not blocked on condition variable and we've unlocked the lock (or at least, did our best to do so),
	// and set the termination state: Go and die peacefully...
	return;
}

/*	PsychFlipWindowBuffersIndirect()
 *
 *	This is a wrapper around PsychFlipWindowBuffers(); which gets all flip request parameters
 *	passed in a struct PsychFlipInfoStruct, decodes that struct, calls the PsychFlipWindowBuffers()
 *	accordingly, then encodes the returned flip results into the struct.
 *
 *	This method not only allows synchronous flips - in which case its the same behaviour
 *	as PsychFlipWindowBuffers(), just with struct parameters - but also asynchronous flips:
 *	In that case, the flip request is just scheduled for later async, parallel execution by
 *	a background helper thread. Another invocation allows to retrieve the results of that
 *	flip synchronously.
 *
 *	This is the preferred method of calling flips from userspace, used in SCREENFlip.c for
 *	standard Screen('Flips'), but also for async Screen('FlipAsyncStart') and Screen('FlipAsyncEnd').
 *
 *	Btw.: Async flips are only supported on Unix operating systems: GNU/Linux and Apple MacOS/X.
 *
 *	The passed windowRecord of the onscreen window to flip must contain a PsychFlipInfoStruct*
 *	flipRequest with all neccessary info for the flip parameters and the fields in which result
 *	shall be returned, as well as the datastructures for thread/mutex/cond locking etc...
 *
 *  flipRequest->opmode can be one of:
 *	0 = Execute Synchronous flip, 1 = Start async flip, 2 = Finish async flip, 3 = Poll for finish of async flip.
 *
 *  *	Synchronous flips are performed without changing the mutex lock flipRequest->performFlipLock. We check if
 *		there are not flip ops scheduled or executing for the window, then simply execute the flip and return its
 *		results, if none are active.
 *
 *	*	Asynchronous flips are always started with the flipInfo struct setup with all needed parameters and
 *		the performFlipLock locked on triggering the worker thread: Either because we create the thread at
 *		first invocation and acquire the lock just before initial trigger, or because we are initiating a flip
 *		after a previous successfull and finished async flip -- in which case we come from there with the lock
 *		held. Also the worker thread is waiting on the flipperGoGoGo condition variable.
 *
 *	*	Async flips are finalized or polled for finalization (and then finalized on poll success) by entering with
 *		the lock not held, so we need to lock->check->unlock (in not ready yet case) or lock->check->finalize in
 *		success case - in which case we leave with the worker thread waiting on the flipperGoGoGo for new work and
 *		our performFlipLock held -- Just as we want it for the next async flip invocation.
 *
 *  More important stuff:
 *
 *  *	Code executing in the PsychFlipperThreadMain() worker thread is not allowed to print anything to the
 *		Matlab/Octave console, alloate or deallocate memory or other stuff that might interact with the runtime
 *		environment Matlab or Octave. We don't know if they are thread-safe, but assume they are not!
 *
 *	*	Error handling as well as clear Screen and Screen('Close', window) or Screen('CloseAll') all trigger
 *		PsychCloseWindow() for the onscreen window, which in turn triggers cleanup in PsychReleaseFlipInfoStruct().
 *		That routine must not only release the struct, but also make absolutely sure that our thread gets cancelled
 *		or signalled to exit and joined, then destroyed and all mutexes unlocked and destroyed!!!
 *
 *  *	The master interpreter thread must detach from the PTB internal OpenGL context for the windowRecord and
 *		not reattach until an async flip is finished! PsychSetGLContext() contains appropriate checking code:
 *		Only one thread is allowed to attach to a specific context, so we must basically lock that ressource as
 *		long as our flipperThread needs it to perform preflip,bufferswap and timestamping, postflip operations...
 *
 *  *	The userspace OpenGL context is not so critical in theory, but we protect that one as well, as it is a
 *		separate context, so no problems from the OpenGL/OS expected (multiple threads can have multiple contexts
 *		attached, as long as each context only has one thread attached), but both contexts share the same drawable
 *		and therefore the same backbuffer. That could prevent bufferswaps at requested deadline/VSYNC because some
 *		usercode rasterizes into the backbuffer and subverts our preflip operations...
 *
 *	Returns success state: TRUE on success, FALSE on error.
 *
 */
bool PsychFlipWindowBuffersIndirect(PsychWindowRecordType *windowRecord)
{
	int rc;
	PsychFlipInfoStruct* flipRequest;
	
	if (NULL == windowRecord) PsychErrorExitMsg(PsychError_internal, "NULL-Ptr for windowRecord passed in PsychFlipWindowsIndirect()!!");
	
	flipRequest = windowRecord->flipInfo;
	if (NULL == flipRequest) PsychErrorExitMsg(PsychError_internal, "NULL-Ptr for 'flipRequest' field of windowRecord passed in PsychFlipWindowsIndirect()!!");

	// Synchronous flip requested?
	if (flipRequest->opmode == 0) {
		// Yes. Any pending operation in progress?
		if (flipRequest->asyncstate != 0) PsychErrorExitMsg(PsychError_internal, "Tried to invoke synchronous flip while flip still in progress!");
		
		// Unpack struct and execute synchronous flip:
		flipRequest->vbl_timestamp = PsychFlipWindowBuffers(windowRecord, flipRequest->multiflip, flipRequest->vbl_synclevel, flipRequest->dont_clear, flipRequest->flipwhen, &(flipRequest->beamPosAtFlip), &(flipRequest->miss_estimate), &(flipRequest->time_at_flipend), &(flipRequest->time_at_onset));

		// Done, and all return values filled in struct. We leave asyncstate at its zero setting, ie., idle and simply return:
		return(TRUE);
	}

#if PSYCH_SYSTEM != PSYCH_WINDOWS

	// Asynchronous flip mode, either request to trigger one or request to finalize one:
	if (flipRequest->opmode == 1) {
		// Async flip start request:
		if (flipRequest->asyncstate != 0) PsychErrorExitMsg(PsychError_internal, "Tried to invoke asynchronous flip while flip still in progress!");

		// Current multiflip > 0 implementation is not thread-safe, so we don't support this:
		if (flipRequest->multiflip != 0)  PsychErrorExitMsg(PsychError_user, "Using a non-zero 'multiflip' flag while starting an asynchronous flip! This is forbidden! Aborted.\n");

		// PsychPreflip operations are not thread-safe due to possible callbacks into Matlab interpreter thread
		// as part of hookchain processing when the imaging pipeline is enabled: We perform/trigger them here
		// before entering the async flip thread:
		PsychPreFlipOperations(windowRecord, flipRequest->dont_clear);

		// Tell Flip that pipeline - flushing has been done already to avoid redundant flush'es:
		windowRecord->PipelineFlushDone = TRUE;

		// ... and flush the pipe:
		glFlush();

		// First time async request? Threads already set up?
		if (flipRequest->flipperThread == NULL) {
			// First time init: Need to startup flipper thread:

			// printf("IN THREADCREATE\n"); fflush(NULL);

			// Create & Init the two mutexes:
			if ((rc=pthread_mutex_init(&(flipRequest->performFlipLock), NULL))) {
				printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): Could not create performFlipLock mutex lock [%s].\n", strerror(rc));
				PsychErrorExitMsg(PsychError_system, "Insufficient system ressources for mutex creation as part of async flip setup!");
			}
			
			if ((rc=pthread_cond_init(&(flipRequest->flipperGoGoGo), NULL))) {
				printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): Could not create flipperGoGoGo condition variable [%s].\n", strerror(rc));
				PsychErrorExitMsg(PsychError_system, "Insufficient system ressources for condition variable creation as part of async flip setup!");
			}
			
			// Set initial thread state to "inactive, not initialized at all":
			flipRequest->flipperState = 0;
			
			// Create and startup thread:
			if ((rc=pthread_create(&(flipRequest->flipperThread), NULL, PsychFlipperThreadMain, (void*) windowRecord))) {
				printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): Could not create flipper  [%s].\n", strerror(rc));
				PsychErrorExitMsg(PsychError_system, "Insufficient system ressources for mutex creation as part of async flip setup!");
			}

			//printf("ENTERING THREADCREATEFINISHED MUTEX\n"); fflush(NULL);
			
			// The thread is started with flipperState == 0, ie., not "initialized and ready", the lock is unlocked.
			// First thing the thread will do is try to lock the lock, then set its flipperState to 1 == initialized and
			// ready, then init itself, then enter a wait on our flipperGoGoGo condition variable and atomically unlock
			// the lock.
			// We now need to try to acquire the lock, then - after we got it - check if we got it because we were faster
			// than the flipperThread and he didn't have a chance to get it (iff flipperState still == 0) - in which case
			// we need to release the lock, wait a bit and then retry a lock->check->sleep->unlock->... cycle. If we got it
			// because flipperState == 1 then this means the thread had the lock, initialized itself, set its state to ready
			// and went sleeping and releasing the lock (that's why we could lock it). In that case, the thread is ready to
			// do work for us and is just waiting for us. At that point we: a) Have the lock, b) can trigger the thread via
			// condition variable to do work for us. That's the condition we want and we can proceed as in the non-firsttimeinit
			// case...
			while (TRUE) {
				// Try to lock, block until available if not available:
			
				//printf("ENTERING THREADCREATEFINISHED MUTEX: MUTEX_LOCK\n"); fflush(NULL);

				if ((rc=pthread_mutex_lock(&(flipRequest->performFlipLock)))) {
					printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): First mutex_lock in init failed  [%s].\n", strerror(rc));
					PsychErrorExitMsg(PsychError_system, "Internal error or deadlock avoided as part of async flip setup!");
				}
				
				//printf("ENTERING THREADCREATEFINISHED MUTEX: MUTEX_LOCKED!\n"); fflush(NULL);

				// Got it! Check condition:
				if (flipRequest->flipperState == 1) {
					// Thread ready and we have the lock: Proceed...
					break;
				}

				//printf("ENTERING THREADCREATEFINISHED MUTEX: MUTEX_UNLOCK\n"); fflush(NULL);

				if ((rc=pthread_mutex_unlock(&(flipRequest->performFlipLock)))) {
					printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): First mutex_unlock in init failed  [%s].\n", strerror(rc));
					PsychErrorExitMsg(PsychError_system, "Internal error or deadlock avoided as part of async flip setup!");
				}

				//printf("ENTERING THREADCREATEFINISHED MUTEX: MUTEX_UNLOCKED\n"); fflush(NULL);

				// Thread not ready. Sleep a millisecond and repeat...
				PsychWaitIntervalSeconds(0.001);

				//printf("ENTERING THREADCREATEFINISHED MUTEX: RETRY\n"); fflush(NULL);
			}

			// End of first-time init for this windowRecord and its thread.

			// printf("FIRST TIME INIT DONE\n"); fflush(NULL);
		}
		
		// Our flipperThread is ready to do work for us (waiting on flipperGoGoGo condition variable) and
		// we have the lock on the flipRequest struct. The struct is already filled with all input parameters
		// for a flip request, so we can simply release our lock and signal the thread that it should do its
		// job:

		// printf("IN ASYNCSTART: DROP CONTEXT\n"); fflush(NULL);

		// Detach from our OpenGL context:
		PsychSetDrawingTarget(NULL);
		PsychOSUnsetGLContext(windowRecord);
		
		// Increment the counter asyncFlipOpsActive:
		asyncFlipOpsActive++;

		// printf("IN ASYNCSTART: MUTEXUNLOCK\n"); fflush(NULL);

		// Release the lock: This is not strictly needed, as the ptrhead_cond_signal() semantics
		// would automatically take the lock from us and assign it to the signalled thread.
		// TODO CHECK: Might be even better to not unlock for more deterministic flow of scheduling...
		if ((rc=pthread_mutex_unlock(&(flipRequest->performFlipLock)))) {
			printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): mutex_unlock in trigger operation failed  [%s].\n", strerror(rc));
			PsychErrorExitMsg(PsychError_internal, "This must not ever happen! PTB design bug or severe operating system or runtime environment malfunction!! Memory corruption?!?");
		}

		// printf("IN ASYNCSTART: MUTEXUNLOCKED -- SIGNALLING %s\n", strerror(rc)); fflush(NULL);
		
		// Trigger the thread:
		if ((rc=pthread_cond_signal(&(flipRequest->flipperGoGoGo)))) {
			printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): pthread_cond_signal in trigger operation failed  [%s].\n", strerror(rc));
			PsychErrorExitMsg(PsychError_internal, "This must not ever happen! PTB design bug or severe operating system or runtime environment malfunction!! Memory corruption?!?");
		}

		// printf("IN ASYNCSTART: MUTEXUNLOCKED -- SIGNALLED -- ASYNCSTATE 1 -- RETURNING\n"); fflush(NULL);
		
		// That's it, operation in progress: Mark it as such.
		flipRequest->asyncstate = 1;
		
		// Done.
		return(TRUE);
	}
	
	// Request to wait or poll for finalization of an async flip operation:
	if ((flipRequest->opmode == 2) || (flipRequest->opmode == 3)) {
		// Child protection:
		if (flipRequest->asyncstate != 1) PsychErrorExitMsg(PsychError_internal, "Tried to invoke end of an asynchronous flip although none is in progress!");

		// We try to get the lock, then check if flip is finished. If not, we need to wait
		// a bit and retry:
		while (TRUE) {
			if (flipRequest->opmode == 2) {
				// Blocking wait:
				// Try to lock, block until available if not available:

				//printf("END: MUTEX_LOCK\n"); fflush(NULL);
				
				if ((rc=pthread_mutex_lock(&(flipRequest->performFlipLock)))) {
					printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): mutex_lock in wait for finish failed  [%s].\n", strerror(rc));
					PsychErrorExitMsg(PsychError_system, "Internal error or deadlock avoided as part of async flip end!");
				}
			}
			else {
				// Polling mode:
				// Try to lock, fail if not available:

				//printf("END: MUTEX_TRYLOCK: %i\n", flipRequest->flipperState); fflush(NULL);

				if ((rc=pthread_mutex_trylock(&(flipRequest->performFlipLock)))) {
					// Failed. If errno == EBUSY then the lock is busy and we exit our polling operation totally:
					if(rc == EBUSY) return(FALSE);
					
					// Otherwise bad things happened!
					printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): mutex_lock in poll for finish failed  [%s].\n", strerror(rc));
					PsychErrorExitMsg(PsychError_system, "Internal error or deadlock avoided as part of async flip poll end!");
				}
			}

			// printf("END: MUTEX_LOCKED\n"); fflush(NULL);
			
			// Got it! Check condition:
			if (flipRequest->flipperState == 3) {
				// Thread finished with flip request execution, ready for new work and waiting for a trigger and we have the lock: Proceed...
				break;
			}

			//printf("END: NOTREADY MUTEX_UNLOCK\n"); fflush(NULL);

			// Not finished. Unlock:
			if ((rc=pthread_mutex_unlock(&(flipRequest->performFlipLock)))) {
				printf("PTB-ERROR: In Screen('FlipAsyncBegin'): PsychFlipWindowBuffersIndirect(): mutex_unlock in wait/poll for finish failed  [%s].\n", strerror(rc));
				PsychErrorExitMsg(PsychError_system, "Internal error or deadlock avoided as part of async flip end!");
			}

			//printf("END: NOTREADY MUTEX_UNLOCKED\n"); fflush(NULL);

			if (flipRequest->opmode == 3) {
				// Polling mode: We just exit our polling op, so user code can retry later:
				return(FALSE);
			}
			
			//printf("END: RETRY\n"); fflush(NULL);
			
			// Waiting mode, need to repeat:
			// Thread not finished. Sleep a millisecond and repeat...
			PsychWaitIntervalSeconds(0.001);
		}

		//printf("END: SUCCESS\n"); fflush(NULL);
		
		// Ok, the thread is finished and ready for new work and waiting.
		// We have the lock as well.
		
		// Reset thread state to "initialized, ready and waiting" just as if it just started at first invocation:
		flipRequest->flipperState = 1;

		// Set flip state to finished:
		flipRequest->asyncstate = 2;
		
		// Decrement the asyncFlipOpsActive count:
		asyncFlipOpsActive--;

		// Now we are in the same condition as after first time init. The thread is waiting for new work,
		// we hold the lock so we can read out the flipRequest struct or fill it with a new request,
		// and all information from the finalized flip is available in the struct.
		// We can return to our parent function with our result:
		return(TRUE);
	}

#else

	// MS-Windows: Async flip ops not supported:
	PsychErrorExitMsg(PsychError_unimplemented, "Sorry, asynchronous display flip operations are not supported on inferior operating systems.");

#endif

	return(TRUE);
}

/*
    PsychFlipWindowBuffers()
    
    Flip front and back buffers in sync with vertical retrace (VBL) and sync Matlab to VBL.
    Returns various timestamps related to sync to VBL, so experimenters can check proper
    syncing and presentation timing by themselves. Also contains an automatic "skipped frames"
    /"missed presentation deadline" detector, which is pretty reliable.
    Allows to not clear the framebuffer after flip, but keep it as before flip - allows
    incremental drawing of stims. Allows to flip not at next retrace, but at the retrace
    immediately after some deadline (spec'd in system time) has been reached.
    Optimizes rendering in collaboration with new SCREENDrawingFinished.c.
 
    Accepts:
    dont_clear flag: 0 = glClear after flip, 1 = restore backbuffer after flip, 2 = don't do anything.
    flipwhen: 0 = traditional flip (flip on next VBL), >0 flip at VBL immediately after time "flipwhen" has been reached.
             -1 = don't sync PTB's execution to VBL, aka sync stimulus onset to VBL but don't pause execution up to then
                  This also disables all timestamping and deadline checking code and makes synchronization of Matlabs
                  execution locked to the VBL impossible. -> Only useful for very special cases...
    
    Returns:
    double value VBL start time: Calculated time of VBL start (aka buffer swap) from timestamp, beamposition and IFI.
    beamPosAtFlip = Position of monitor scanning beam at time that timestamp was taken.
    miss_estimate = Estimate of how far we've missed our presentation deadline. >0 == No deadline-miss, < 0 == deadline-miss
    time_at_onset = Estimated time when stimulus onset, aka end of VBL, aka beamposition==0 occurs.
    time_at_flipend = Timestamp taken shortly before return of FlipWindowBuffers for benchmarking.
 
*/
double PsychFlipWindowBuffers(PsychWindowRecordType *windowRecord, int multiflip, int vbl_synclevel, int dont_clear, double flipwhen, int* beamPosAtFlip, double* miss_estimate, double* time_at_flipend, double* time_at_onset)
{
    int screenheight, screenwidth;
    GLint read_buffer, draw_buffer;
    unsigned char bufferstamp;
    const boolean vblsyncworkaround=false;  // Setting this to 'true' would enable some checking code. Leave it false by default.
    static unsigned char id=1;
    boolean sync_to_vbl;                    // Should we synchronize the CPU to vertical retrace? 
    double tremaining;                      // Remaining time to flipwhen - deadline
    CGDirectDisplayID displayID;            // Handle for our display - needed for beampos-query.
    double time_at_vbl=0;                   // Time (in seconds) when last Flip in sync with start of VBL happened.
    double currentflipestimate;             // Estimated video flip interval in seconds at current monitor frame rate.
    double currentrefreshestimate;          // Estimated video refresh interval in seconds at current monitor frame rate.
    double tshouldflip;                     // Deadline for a successfull flip. If time_at_vbl > tshouldflip --> Deadline miss!
    double slackfactor;                     // Slack factor for deadline miss detection.
    double vbl_startline;
    double vbl_endline;
    double vbl_lines_elapsed, onset_lines_togo;
    double vbl_time_elapsed; 
    double onset_time_togo;
    long scw, sch;
    psych_uint64 preflip_vblcount = 0;          // VBL counters and timestamps acquired from low-level OS specific routines.
    psych_uint64 postflip_vblcount = 0;         // Currently only supported on OS-X, but Linux/X11 implementation will follow.
    double preflip_vbltimestamp = -1;
    double postflip_vbltimestamp = -1;
	unsigned int vbltimestampquery_retrycount = 0;
	double time_at_swaprequest=0;			// Timestamp taken immediately before requesting buffer swap. Used for consistency checks.
	double time_post_swaprequest=0;			// Timestamp taken immediately after requesting buffer swap. Used for consistency checks.
	double time_at_swapcompletion=0;		// Timestamp taken after swap completion -- initially identical to time_at_vbl.
	int line_pre_swaprequest = -1;			// Scanline of display immediately before swaprequest.
	int line_post_swaprequest = -1;			// Scanline of display immediately after swaprequest.
	int min_line_allowed = 20;				// The scanline up to which "out of VBL" swaps are accepted: A fudge factor for broken drivers...
	boolean flipcondition_satisfied;	
    int vbltimestampmode = PsychPrefStateGet_VBLTimestampingMode();
    PsychWindowRecordType **windowRecordArray=NULL;
    int	i;
    int numWindows=0; 
	int verbosity = PsychPrefStateGet_Verbosity();

    // Child protection:
    if(windowRecord->windowType!=kPsychDoubleBufferOnscreen)
        PsychErrorExitMsg(PsychError_internal,"Attempt to swap a single window buffer");
    
    // Retrieve estimate of interframe flip-interval:
    if (windowRecord->nrIFISamples > 0) {
        currentflipestimate=windowRecord->IFIRunningSum / ((double) windowRecord->nrIFISamples);
    }
    else {
        // We don't have a valid estimate! This will screw up all timestamping, checking and waiting code!
        // It also indicates that syncing to VBL doesn't work!
        currentflipestimate=0;
        // We abort - This is too unsafe...
        PsychErrorExitMsg(PsychError_internal,"Flip called, while estimate of monitor flip interval is INVALID -> Syncing trouble -> Aborting!");
    }
    
    // Retrieve estimate of monitor refresh interval:
    if (windowRecord->VideoRefreshInterval > 0) {
        currentrefreshestimate = windowRecord->VideoRefreshInterval;
    }
    else {
        currentrefreshestimate=0;
        // We abort - This is too unsafe...
        PsychErrorExitMsg(PsychError_internal,"Flip called, while estimate of monitor refresh interval is INVALID -> Syncing trouble -> Aborting!");
    }
    
    // Setup reasonable slack-factor for deadline miss detector:
    if (windowRecord->VBL_Endline!=-1) {
        // If beam position queries work, we use a tight value:
        slackfactor = 1.05;
    }
    else {
        // If beam position queries don't work, we use a "slacky" value:
        slackfactor = 1.2;
    }
    
    // Retrieve display id and screen size spec that is needed later...
    PsychGetCGDisplayIDFromScreenNumber(&displayID, windowRecord->screenNumber);
    screenwidth=(int) PsychGetWidthFromRect(windowRecord->rect);
    screenheight=(int) PsychGetHeightFromRect(windowRecord->rect);
    // Query real size of the underlying display in order to define the vbl_startline:
    PsychGetScreenSize(windowRecord->screenNumber, &scw, &sch);
    vbl_startline = (int) sch;

    // Enable this windowRecords framebuffer as current drawingtarget:
    PsychSetDrawingTarget(windowRecord);
    
    // Should we sync to the onset of vertical retrace?
    // Note: Flipping the front- and backbuffers is nearly always done in sync with VBL on
    // a double-buffered setup. sync_to_vbl specs, if the application should wait for
    // the VBL to start before continuing execution.
    sync_to_vbl = (vbl_synclevel == 0 || vbl_synclevel == 3) ? true : false;
    
    if (vbl_synclevel==2) {
        // We are requested to flip immediately, instead of syncing to VBL. Disable VBL-Sync.
		PsychOSSetVBLSyncLevel(windowRecord, 0);
		// Disable also for a slave window, if any:
		if (windowRecord->slaveWindow) PsychOSSetVBLSyncLevel(windowRecord->slaveWindow, 0);
    }
    
    if (multiflip > 0) {
        // Experimental Multiflip requested. Build list of all onscreen windows...
		// CAUTION: This is not thread-safe in the Matlab/Octave environment, due to
		// callbacks into Matlabs/Octaves memory managment from a non-master thread!
		// --> multiflip > 0 is not allowed for async flips!!!
        PsychCreateVolatileWindowRecordPointerList(&numWindows, &windowRecordArray);
    }
    
    if (multiflip == 2) {
        // Disable VBL-Sync for all onscreen windows except our primary one:
        for(i=0;i<numWindows;i++) {
            if (PsychIsOnscreenWindow(windowRecordArray[i]) && (windowRecordArray[i]!=windowRecord)) {
				PsychOSSetVBLSyncLevel(windowRecordArray[i], 0);
            }
        }
    }
    
    // Backup current assignment of read- writebuffers:
    glGetIntegerv(GL_READ_BUFFER, &read_buffer);
    glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
    
    // Perform preflip-operations: Backbuffer backups for the different dontclear-modes
    // and special compositing operations for specific stereo algorithms...
	// These are not thread-safe! For async flip, these must be called in async flip start
	// while still on the main thread, so this call here turns into a no-op:
    PsychPreFlipOperations(windowRecord, dont_clear);
    
	// Reset color write mask to "all enabled"
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

	// Special imaging mode active? in that case a FBO may be bound instead of the system framebuffer.
	if (windowRecord->imagingMode > 0) {
		// Reset our drawing engine: This will unbind any FBO's (i.e. reset to system framebuffer)
		// and reset the current target window to 'none'. We need this to be sure that our flip
		// sync pixel token is written to the real system backbuffer...
		PsychSetDrawingTarget(NULL);
	}
	
    // Part 1 of workaround- /checkcode for syncing to vertical retrace:
    if (vblsyncworkaround) {
        glDrawBuffer(GL_BACK);
        glRasterPos2f(0, screenheight);
        glDrawPixels(1,1,GL_RED,GL_UNSIGNED_BYTE, &id);
    }
 
    // Pausing until a specific deadline requested?
    if (flipwhen>0) {
        // We shall not swap at next VSYNC, but at the VSYNC immediately following the
        // system time "flipwhen". This is the premium version of the old WaitBlanking... ;-)
        
        // Calculate deadline for a successfull flip: If time_at_vbl is later than that,
        // it means that we missed the proper video refresh cycle:
        tshouldflip = flipwhen;
        
        // Some time left until deadline 'flipwhen'?
        PsychGetAdjustedPrecisionTimerSeconds(&tremaining);
        if ((flipwhen - tremaining) > 0) {
            // Child protection against people specifying a flipwhen that's infinite, e.g.,
            // via wrong ifi calculation in Matlab: if a flipwhen more than 1000 secs.
            // in the future is specified, we just assume this is an error...
            if (flipwhen - tremaining > 1000) {
                PsychErrorExitMsg(PsychError_user, "\nYou specified a 'when' value to Flip that's over 1000 seconds in the future?!? Aborting, assuming that's an error.\n\n");
            }
            
            // We force the rendering pipeline to finish all pending OpenGL operations,
            // so we can be sure that swapping at VBL will be as fast as possible.
            // Btw: glFlush() is not recommended by Apple, but in this specific case
            // it makes sense (MK). We avoid this redundant flush, if a pipeline flush has
            // been already done by other routines, e.g, SCREENDrawingFinished.
            if (!windowRecord->PipelineFlushDone) glFlush();

            // We'll sleep - and hope that the OS will wake us up in time, if the remaining waiting
            // time is more than 0 milliseconds. This way, we don't burn up valuable CPU cycles by
            // busy waiting and don't get punished by the overload detection of the OS:
            PsychWaitUntilSeconds(flipwhen);
        }
        // At this point, we are less than one video refresh interval away from the deadline - the next
        // VBL will be the one we want to flip at. Leave the rest of the job to CGLFlushDrawable...
    }
    else {
        // We should lock to next possible VSYNC:
        // Calculate deadline for a successfull flip on next VSYNC: If time_at_vbl is later than that,
        // it means that we missed the proper video refresh cycle:
        PsychGetAdjustedPrecisionTimerSeconds(&tshouldflip);        
    }

    // Do we know the exact system time when a VBL happened in the past?
    if ((windowRecord->time_at_last_vbl > 0) && (currentflipestimate > 0)) {
      // Yes! We use this as a base-line time to compute from the current time a virtual deadline,
      // which is at the beginning of the current monitor refresh interval.
      //
      // As long as we do synchronous Flips (sync_to_vbl == true - PTB blocks until VBL onset),
      // we should have a valid time_at_last_vbl, so this mechanism works.
      // Only on the *very first* invocation of Flip either after PTB-Startup or after a non-blocking
      // Flip, we can't do this because the time_at_last_vbl timestamp isn't available...
      tshouldflip = windowRecord->time_at_last_vbl + ((floor((tshouldflip - windowRecord->time_at_last_vbl) / currentflipestimate)) * currentflipestimate);
    }
    
    // Calculate final deadline for deadline-miss detection:
    tshouldflip = tshouldflip + slackfactor * currentflipestimate;        
    
    // Update of hardware gamma table in sync with flip requested?
    if ((windowRecord->inRedTable) && (windowRecord->loadGammaTableOnNextFlip > 0)) {
      // Yes! Call the update routine now. It should schedule the actual update for
      // the same VSYNC to which our bufferswap will lock. "Should" means, we have no
      // way of checking programmatically if it really worked, only via our normal deadline
      // miss detector. If we are running under M$-Windows then loading the hw-gamma table
      // will block our execution until next retrace. Then it will upload the new gamma table.
      // Therefore we need to disable sync of bufferswaps to VBL, otherwise we would swap only
      // one VBL after the table update -> out of sync by one monitor refresh!
      if (PSYCH_SYSTEM==PSYCH_WINDOWS) PsychOSSetVBLSyncLevel(windowRecord, 0);
      
      // We need to wait for render-completion here, otherwise hw-gamma table update and
      // bufferswap could get out of sync due to unfinished rendering commands which would
      // defer the bufferswap, but not the clut update.
      glFinish();
      
      // Perform hw-table upload on M$-Windows in sync with retrace, wait until completion. On
      // OS-X just schedule update in sync with next retrace, but continue immediately:
      PsychLoadNormalizedGammaTable(windowRecord->screenNumber, windowRecord->inTableSize, windowRecord->inRedTable, windowRecord->inGreenTable, windowRecord->inBlueTable);
    }
	else {
		// Need to sync the pipeline, if this special workaround is active to get good timing:
		if (PsychPrefStateGet_ConserveVRAM() & kPsychBusyWaitForVBLBeforeBufferSwapRequest) glFinish();
	}

	// Low level queries to the driver:
	// We query the timestamp and count of the last vertical retrace. This is needed for
	// correctness checking and timestamp computation on gfx-hardware without beamposition
	// queries (IntelMacs as of OS/X 10.4.10).
	// In frame-sequential stereo mode it also allows to lock bufferswaps either to even
	// or odd video refresh intervals (if windowRecord->targetFlipFieldType specifies this).
	// That way one can require stereo stimulus onset with either the left eye view or the
	// right eye view, depending on flip field selection. In other stereo modes or mono
	// mode one usually doesn't care about onset in even or odd fields.
	flipcondition_satisfied = FALSE;
	do {
		// Query driver:
		preflip_vbltimestamp = PsychOSGetVBLTimeAndCount(windowRecord->screenNumber, &preflip_vblcount);
		// Check if ready for flip, ie. if the proper even/odd video refresh cycle is approaching or
		// if we don't care about this:
		flipcondition_satisfied = (windowRecord->targetFlipFieldType == -1) || (((preflip_vblcount + 1) % 2) == windowRecord->targetFlipFieldType);
		// If in wrong video cycle, we simply sleep a millisecond, then retry...
		if (!flipcondition_satisfied) PsychWaitIntervalSeconds(0.001);
	} while (!flipcondition_satisfied);
    
	// Take a measurement of the beamposition at time of swap request:
	line_pre_swaprequest = (int) PsychGetDisplayBeamPosition(displayID, windowRecord->screenNumber);

	// Take preswap timestamp:
	PsychGetAdjustedPrecisionTimerSeconds(&time_at_swaprequest);

	// Some check for buggy drivers: If VBL synched flipping is requested, we expect that at least 2 msecs
	// should pass between consecutive bufferswaps. 2 msecs is chosen because the VBL period of most displays
	// at most settings does not last longer than 2 msecs (usually way less than 1 msec), and this would still allow
	// for an update rate of 500 Hz -- more than any current display can do.
	if ((windowRecord->time_at_last_vbl > 0) && (vbl_synclevel!=2) && (time_at_swaprequest - windowRecord->time_at_last_vbl < 0.002)) {
		// Less than 2 msecs passed since last bufferswap, although swap in sync with retrace requested.
		// Some drivers seem to have a bug where a bufferswap happens anywhere in the VBL period, even
		// if already a swap happened in a VBL --> Multiple swaps per refresh cycle if this routine is
		// called fast enough, ie. multiple times during one single VBL period. Not good!
		// An example is the ATI Mobility Radeon X1600 in 2nd generation MacBookPro's under OS/X 10.4.10
		// and 10.4.11 -- probably most cards operated by the same driver have the same problem...
		//
		// We try to enforce correct behaviour by waiting until at least 2 msecs have elapsed before the next
		// bufferswap:
		PsychWaitUntilSeconds(windowRecord->time_at_last_vbl + 0.002);

		// Take a measurement of the beamposition at time of swap request:
		line_pre_swaprequest = (int) PsychGetDisplayBeamPosition(displayID, windowRecord->screenNumber);

		// Take updated preswap timestamp:
		PsychGetAdjustedPrecisionTimerSeconds(&time_at_swaprequest);
	}
	
    // Trigger the "Front <-> Back buffer swap (flip) on next vertical retrace" and
    PsychOSFlipWindowBuffers(windowRecord);
	
	// Also swap the slave window, if any:
	if (windowRecord->slaveWindow) PsychOSFlipWindowBuffers(windowRecord->slaveWindow);

    // Multiflip with vbl-sync requested?
    if (multiflip==1) {
        //  Trigger the "Front <-> Back buffer swap (flip) on next vertical retrace"
        //  for all onscreen windows except our primary one:
        for(i=0;i<numWindows;i++) {
            if (PsychIsOnscreenWindow(windowRecordArray[i]) && (windowRecordArray[i]!=windowRecord)) {
                PsychOSFlipWindowBuffers(windowRecordArray[i]);
            }
        }
    }
    
	// Take post-swap request line:
	line_post_swaprequest = (int) PsychGetDisplayBeamPosition(displayID, windowRecord->screenNumber);

	// Take postswap request timestamp:
	PsychGetAdjustedPrecisionTimerSeconds(&time_post_swaprequest);

    // Pause execution of application until start of VBL, if requested:
    if (sync_to_vbl) {
        if ((vbl_synclevel==3) && (windowRecord->VBL_Endline != -1)) {
            // Wait for VBL onset via experimental busy-waiting spinloop instead of
            // blocking: We spin-wait until the rasterbeam of our master-display enters the
            // VBL-Area of the display:
            while (vbl_startline > (int) PsychGetDisplayBeamPosition(displayID, windowRecord->screenNumber));
        }
        else {
            // Standard blocking wait for flip/VBL onset requested:
            
            // Draw a single pixel in left-top area of back-buffer. This will wait/stall the rendering pipeline
            // until the buffer flip has happened, aka immediately after the VBL has started.
            // We need the pixel as "synchronization token", so the following glFinish() really
            // waits for VBL instead of just "falling through" due to the asynchronous nature of
            // OpenGL:
            glDrawBuffer(GL_BACK_LEFT);
            // We draw our single pixel with an alpha-value of zero - so effectively it doesn't
            // change the color buffer - just the z-buffer if z-writes are enabled...
            glColor4f(0,0,0,0);
            glBegin(GL_POINTS);
            glVertex2i(10,10);
            glEnd();
            // This glFinish() will wait until point drawing is finished, ergo backbuffer was ready
            // for drawing, ergo buffer swap in sync with start of VBL has happened.
            glFinish();
        }
        
        // At this point, start of VBL on masterdisplay has happened and we can continue execution...
        
        // Multiflip without vbl-sync requested?
        if (multiflip==2) {
            // Immediately flip all onscreen windows except our primary one:
            for(i=0;i<numWindows;i++) {
                if (PsychIsOnscreenWindow(windowRecordArray[i]) && (windowRecordArray[i]!=windowRecord)) {
                    PsychOSFlipWindowBuffers(windowRecordArray[i]);
                }
            }
        }
        
        // Query and return rasterbeam position immediately after Flip and before timestamp:
        *beamPosAtFlip=(int) PsychGetDisplayBeamPosition(displayID, windowRecord->screenNumber);

         // We take a timestamp here and return it to "userspace"
        PsychGetAdjustedPrecisionTimerSeconds(&time_at_vbl);
		time_at_swapcompletion = time_at_vbl;

        // Run kernel-level timestamping always in mode > 1 or on demand in mode 1 if beampos.
        // queries don't work properly:
        if (vbltimestampmode > 1 || (vbltimestampmode == 1 && windowRecord->VBL_Endline == -1)) {
            // OS-X only: Low level query to the driver: We need to yield the cpu for a couple of
            // microseconds, let's say 250 microsecs. for now, so the low-level vbl interrupt task
            // in IOKits workloop can do its job. But first let's try to do it without yielding...
			vbltimestampquery_retrycount = 0;
			PsychWaitIntervalSeconds(0.00025);
			postflip_vbltimestamp = PsychOSGetVBLTimeAndCount(windowRecord->screenNumber, &postflip_vblcount);

			// If a valid preflip timestamp equals the postflip timestamp although the swaprequest likely didn't
			// happen inside a VBL interval (in which case this would be a legal condition), we retry the
			// query up to 8 times, each time sleeping for 0.25 msecs, for a total retry time of 2 msecs.
			// The sleeping is meant to release the processor to other system tasks which may be crucial for
			// correct timestamping, but preempted by our Matlab thread in realtime mode. If we don't succeed
			// in 2 msecs then something's pretty screwed and we should just give up.
            while ((preflip_vbltimestamp > 0) && (preflip_vbltimestamp == postflip_vbltimestamp) && (vbltimestampquery_retrycount < 8) && (time_at_swaprequest - preflip_vbltimestamp > 0.001)) {
                PsychWaitIntervalSeconds(0.00025);
                postflip_vbltimestamp = PsychOSGetVBLTimeAndCount(windowRecord->screenNumber, &postflip_vblcount);
				vbltimestampquery_retrycount++;
			}			
        }
        
        // Calculate estimate of real time of VBL, based on our post glFinish() timestamp, post glFinish() beam-
        // position and the roughly known height of image and duration of IFI. The corrected time_at_vbl
        // contains time at start of VBL. This value is crucial for control stimulus presentation timing.
        // We also estimate the end of VBL, aka the stimulus onset time in time_at_onset.
        
        // First we calculate the number of scanlines that have passed since start of VBL area:
        vbl_endline = windowRecord->VBL_Endline;
        
        // VBL_Endline is determined in a calibration loop in PsychOpenOnscreenWindow above.
        // If this fails for some reason, we mark it as invalid by setting it to -1.
        if ((windowRecord->VBL_Endline != -1) && (vbltimestampmode>=0)) {

			// One more sanity check to account for the existence of the most
			// insane OS on earth: Check for impossible beamposition values although
			// we've already verified correct working of the queries during startup.
			if ((*beamPosAtFlip < 0) || (*beamPosAtFlip > vbl_endline)) {
				// Ok, this is completely foo-bared.
				if (verbosity > 0) {
					printf("PTB-ERROR: Beamposition query after flip returned the *impossible* value %i (Valid would be between zero and %i)!!!\n", *beamPosAtFlip, (int) vbl_endline);
					printf("PTB-ERROR: This is a severe malfunction, indicating a bug in your graphics driver. Will disable beamposition queries from now on.\n");
					if ((PSYCH_SYSTEM == PSYCH_OSX) && (vbltimestampmode == 1)) { 
						printf("PTB-ERROR: As this is MacOS/X, i'll switch to a (potentially slightly less accurate) mechanism based on vertical blank interrupts...\n");
					}
					else {
						printf("PTB-ERROR: Timestamps returned by Flip will be correct, but less robust and accurate than they would be with working beamposition queries.\n");
					}
					printf("PTB-ERROR: It's strongly recommended to update your graphics driver and optionally file a bug report to your vendor if that doesn't help.\n");
					printf("PTB-ERROR: Read 'help Beampositionqueries' for further information.\n");
				}
				
				// Mark vbl endline as invalid, so beampos is not used anymore for future flips.
				windowRecord->VBL_Endline = -1;
				
				// Create fake beampos value for this invocation of Flip so we return an ok timestamp:
				*beamPosAtFlip = vbl_startline;
			}
			
            if (*beamPosAtFlip >= vbl_startline) {
                vbl_lines_elapsed = *beamPosAtFlip - vbl_startline;
                onset_lines_togo = vbl_endline - (*beamPosAtFlip) + 1;
            }
            else {
                vbl_lines_elapsed = vbl_endline - vbl_startline + 1 + *beamPosAtFlip;
                onset_lines_togo = -1.0 * (*beamPosAtFlip);
            }
            
            // From the elapsed number we calculate the elapsed time since VBL start:
            vbl_time_elapsed = vbl_lines_elapsed / vbl_endline * currentrefreshestimate; 
            onset_time_togo = onset_lines_togo / vbl_endline * currentrefreshestimate;
            // Compute of stimulus-onset, aka time when retrace is finished:
            *time_at_onset = time_at_vbl + onset_time_togo;
            // Now we correct our time_at_vbl by this correction value:
            time_at_vbl = time_at_vbl - vbl_time_elapsed;
        }
        else {
            // Beamposition queries unavailable!
            
            // Shall we fall-back to kernel-level query?
            if ((vbltimestampmode==1 || vbltimestampmode==2) && preflip_vbltimestamp > 0) {
                // Yes: Use fallback result:
                time_at_vbl = postflip_vbltimestamp;
            }
            
            // If we can't depend on timestamp correction, we just set time_at_onset == time_at_vbl.
            // This is not strictly correct, but at least the user doesn't have to change the whole
            // implementation of his code and we've warned him anyway at Window open time...
            *time_at_onset=time_at_vbl;
        }
                
        // OS level queries of timestamps supported and consistency check wanted?
        if (preflip_vbltimestamp > 0 && vbltimestampmode==2) {
            // Yes. Check both methods for consistency: We accept max. 1 ms deviation.
            if ((fabs(postflip_vbltimestamp - time_at_vbl) > 0.001) || (verbosity > 20)) {
                printf("VBL timestamp deviation: precount=%i , postcount=%i, delta = %i, postflip_vbltimestamp = %lf  -  beampos_vbltimestamp = %lf  == Delta is = %lf \n",
                   (int) preflip_vblcount, (int) postflip_vblcount, (int) (postflip_vblcount - preflip_vblcount), postflip_vbltimestamp, time_at_vbl, postflip_vbltimestamp - time_at_vbl);
            }
        }
        
        // Shall kernel-level method override everything else?
        if (preflip_vbltimestamp > 0 && vbltimestampmode==3) {
            time_at_vbl = postflip_vbltimestamp;
            *time_at_onset=time_at_vbl;
        }
        
		// Another consistency check: Computed swap/VBL timestamp should never be earlier than
		// the system time when bufferswap request was initiated - Can't complete swap before
		// actually starting it! We test for this, but allow for a slack of 50 microseconds,
		// because a small "too early" offset could be just due to small errors in refresh rate
		// calibration or other sources of harmless timing errors.
		//
		// This is a test specific for beamposition based timestamping. We can't execute the
		// test (would not be diagnostic) if the swaprequest happened within the VBL interval,
		// because in that case, it is possible to get a VBL swap timestamp that is before the
		// swaprequest: The timestamp always denotes the onset of a VBL, but a swaprequest issued
		// at the very end of VBL would still get executed, therefore the VBL timestamp would be
		// valid although it technically precedes the time of the "late" swap request: This is
		// why we check the beampositions around time of swaprequest to make sure that the request
		// was issued while outside the VBL:
		if ((time_at_vbl < time_at_swaprequest - 0.00005) && ((line_pre_swaprequest > min_line_allowed) && (line_pre_swaprequest < vbl_startline)) && (windowRecord->VBL_Endline != -1) &&
			((line_post_swaprequest > min_line_allowed) && (line_post_swaprequest < vbl_startline)) && (line_pre_swaprequest <= line_post_swaprequest) &&
			(vbltimestampmode >= 0) && (vbltimestampmode < 3)) {

			// Ohoh! Broken timing. Disable beamposition timestamping for future operations, warn user.			
			if (verbosity > 0) {
				printf("\n\nPTB-ERROR: Screen('Flip'); beamposition timestamping computed an *impossible stimulus onset value* of %lf secs, which would indicate that\n", time_at_vbl);
				printf("PTB-ERROR: stimulus onset happened *before* it was actually requested! (Earliest theoretically possible %lf secs).\n\n", time_at_swaprequest);
				printf("PTB-ERROR: Some more diagnostic values (only for experts): line_pre_swaprequest = %i, line_post_swaprequest = %i, time_post_swaprequest = %lf\n", line_pre_swaprequest, line_post_swaprequest, time_post_swaprequest);
				printf("PTB-ERROR: Some more diagnostic values (only for experts): preflip_vblcount = %i, preflip_vbltimestamp = %lf\n", (int) preflip_vblcount, preflip_vbltimestamp);
				printf("PTB-ERROR: Some more diagnostic values (only for experts): postflip_vblcount = %i, postflip_vbltimestamp = %lf, vbltimestampquery_retrycount = %i\n", (int) postflip_vblcount, postflip_vbltimestamp, (int) vbltimestampquery_retrycount);
				printf("\n");
			}
			
			// Is VBL IRQ timestamping allowed as a fallback and delivered a valid result?
			if (vbltimestampmode >= 1 && postflip_vbltimestamp > 0) {
				// Available. Meaningful result?
				if (verbosity > 0) {
					printf("PTB-ERROR: The most likely cause of this error (based on cross-check with kernel-level timestamping) is:\n");
					if (((postflip_vbltimestamp < time_at_swaprequest - 0.00005) && (postflip_vbltimestamp == preflip_vbltimestamp)) ||
						((preflip_vblcount + 1 == postflip_vblcount) && (vbltimestampquery_retrycount > 1))) {
						// Hmm. These results back up the hypothesis that sync of bufferswaps to VBL is broken, ie.
						// the buffers swap as soon as swappable and requested, instead of only within VBL:
						printf("PTB-ERROR: Synchronization of stimulus onset (buffer swap) to the vertical blank interval VBL is not working properly.\n");
						printf("PTB-ERROR: Please run the script PerceptualVBLSyncTest to check this. With non-working sync to VBL, all stimulus timing\n");
						printf("PTB-ERROR: becomes quite futile. Also read 'help SyncTrouble' !\n");
						printf("PTB-ERROR: For the remainder of this session, i've switched to kernel based timestamping as a backup method for the\n");
						printf("PTB-ERROR: less likely case that beamposition timestamping in your system is broken. However, this method seems to\n");
						printf("PTB-ERROR: confirm the hypothesis of broken sync of stimulus onset to VBL.\n\n");
					}
					else {
						// VBL IRQ timestamping doesn't support VBL sync failure, so it might be a problem with beamposition timestamping...
						printf("PTB-ERROR: Something may be broken in your systems beamposition timestamping. Read 'help SyncTrouble' !\n\n");
						printf("PTB-ERROR: For the remainder of this session, i've switched to kernel based timestamping as a backup method.\n");
						printf("PTB-ERROR: This method is slightly less accurate and higher overhead but should be similarly robust.\n");
						printf("PTB-ERROR: A less likely cause could be that Synchronization of stimulus onset (buffer swap) to the\n");
						printf("PTB-ERROR: vertical blank interval VBL is not working properly.\n");
						printf("PTB-ERROR: Please run the script PerceptualVBLSyncTest to check this. With non-working sync to VBL, all stimulus timing\n");
						printf("PTB-ERROR: becomes quite futile.\n");
					}
				}
				
				// Disable beamposition timestamping for further session:
				windowRecord->VBL_Endline = -1;
				
				// Set vbltimestampmode = 0 for rest of this subfunction, so the checking code for
				// stand-alone kernel level timestamping below this routine gets suppressed for this
				// iteration:
				vbltimestampmode = 0;
				
				// In any case: Override with VBL IRQ method results:
				time_at_vbl = postflip_vbltimestamp;
				*time_at_onset=time_at_vbl;
			}
			else {
				// VBL timestamping didn't deliver results? Because its not enabled in parallel with beampos queries?
				if ((vbltimestampmode == 1) && (PSYCH_SYSTEM == PSYCH_OSX)) {
					// Auto fallback enabled, but not if beampos queries appear to be functional. They are
					// dysfunctional by now, but weren't at swap time, so we can't get any useful data from
					// kernel level timestamping. However in the next round we should get something. Therefore,
					// enable both methods in consistency cross check mode:
					PsychPrefStateSet_VBLTimestampingMode(2);

					// Set vbltimestampmode = 0 for rest of this subfunction, so the checking code for
					// stand-alone kernel level timestamping below this routine gets suppressed for this
					// iteration:
					vbltimestampmode = 0;

					if (verbosity > 0) {
						printf("PTB-ERROR: I have enabled additional cross checking between beamposition based and kernel-level based timestamping.\n");
						printf("PTB-ERROR: This should allow to get a better idea of what's going wrong if successive invocations of Screen('Flip');\n");
						printf("PTB-ERROR: fail to deliver proper timestamps as well. It may even fix the problem if the culprit would be a bug in \n");
						printf("PTB-ERROR: beamposition based high precision timestamping. We will see...\n\n");
						printf("PTB-ERROR: An equally likely cause would be that Synchronization of stimulus onset (buffer swap) to the\n");
						printf("PTB-ERROR: vertical blank interval VBL is not working properly.\n");
						printf("PTB-ERROR: Please run the script PerceptualVBLSyncTest to check this. With non-working sync to VBL, all stimulus timing\n");
						printf("PTB-ERROR: becomes quite futile. Also read 'help SyncTrouble' !\n");
					}
				}
				else {
					// Ok, we lost:
					// VBL kernel level timestamping not operational or intentionally disabled: No backup solutions left, and no way to
					// cross-check stuff: We disable high precision timestamping completely:

					// Disable beamposition timestamping for further session:
					PsychPrefStateSet_VBLTimestampingMode(-1);
					vbltimestampmode = -1;
					
					if (verbosity > 0) {
						printf("PTB-ERROR: This error can be due to either of the following causes (No way to discriminate):\n");
						printf("PTB-ERROR: Either something is broken in your systems beamposition timestamping. I've disabled high precision\n");
						printf("PTB-ERROR: timestamping for now. Returned timestamps will be less robust and accurate, but if that was the culprit it should be fixed.\n\n");
						printf("PTB-ERROR: An equally likely cause would be that Synchronization of stimulus onset (buffer swap) to the\n");
						printf("PTB-ERROR: vertical blank interval VBL is not working properly.\n");
						printf("PTB-ERROR: Please run the script PerceptualVBLSyncTest to check this. With non-working sync to VBL, all stimulus timing\n");
						printf("PTB-ERROR: becomes quite futile. Also read 'help SyncTrouble' !\n");
					}
				}
			}
		}
		
		// VBL IRQ based timestamping in charge?
		if ((PSYCH_SYSTEM == PSYCH_OSX) && ((vbltimestampmode == 3) || (vbltimestampmode > 0 && windowRecord->VBL_Endline == -1))) {
			// Yes. Try some consistency checks for that:

			// Some diagnostics at high debug-levels:
			if (vbltimestampquery_retrycount > 0 && verbosity > 10) printf("PTB-DEBUG: In PsychFlipWindowBuffers(), VBLTimestamping: RETRYCOUNT %i : Delta Swaprequest - preflip_vbl timestamp: %lf secs.\n", (int) vbltimestampquery_retrycount, time_at_swaprequest - preflip_vbltimestamp);

			if ((vbltimestampquery_retrycount>=8) && (preflip_vbltimestamp == postflip_vbltimestamp) && (preflip_vbltimestamp > 0)) {
				// Postflip timestamp equals valid preflip timestamp after many retries:
				// This can be due to a swaprequest emitted and satisfied/completed within a single VBL
				// interval - then it would be perfectly fine. Or it happens despite a swaprequest in
				// the middle of a video refersh cycle. Then it would indicate trouble, either with the
				// timestamping mechanism or with sync of bufferswaps to VBL:
				// If we happened to do everything within a VBL, then the different timestamps should
				// be close together -- probably within 2 msecs - the max duration of a VBL and/or retry sequence:
				if (fabs(preflip_vbltimestamp - time_at_swaprequest) < 0.002) {
					// Swaprequest, Completion and whole timestamping happened likely within one VBL,
					// so no reason to worry...
					if (verbosity > 10) {
						printf("PTB-DEBUG: With kernel-level timestamping: ");
						printf("vbltimestampquery_retrycount = %i, preflip_vbltimestamp=postflip= %lf, time_at_swaprequest= %lf\n", (int) vbltimestampquery_retrycount, preflip_vbltimestamp, time_at_swaprequest);
					}
				}
				else {
					// Stupid values, but swaprequest not close to VBL, but likely within refresh cycle.
					// This could be either broken queries, or broken sync to VBL:
					if (verbosity > 0) {
						printf("\n\nPTB-ERROR: Screen('Flip'); kernel-level timestamping computed bogus values!!!\n");
						printf("PTB-ERROR: vbltimestampquery_retrycount = %i, preflip_vbltimestamp=postflip= %lf, time_at_swaprequest= %lf\n", (int) vbltimestampquery_retrycount, preflip_vbltimestamp, time_at_swaprequest);
						printf("PTB-ERROR: This error can be due to either of the following causes (No simple way to discriminate):\n");
						printf("PTB-ERROR: Either something is broken in your systems VBL-IRQ timestamping. I've disabled high precision\n");
						printf("PTB-ERROR: timestamping for now. Returned timestamps will be less robust and accurate, but at least ok, if that was the culprit.\n\n");
						printf("PTB-ERROR: An equally likely cause would be that Synchronization of stimulus onset (buffer swap) to the\n");
						printf("PTB-ERROR: vertical blank interval VBL is not working properly.\n");
						printf("PTB-ERROR: Please run the script PerceptualVBLSyncTest to check this. With non-working sync to VBL, all stimulus timing\n");
						printf("PTB-ERROR: becomes quite futile. Also read 'help SyncTrouble' !\n\n\n");
					}
					
					PsychPrefStateSet_VBLTimestampingMode(-1);
					time_at_vbl = time_at_swapcompletion;
					*time_at_onset=time_at_vbl;
				}
			}
									
			// We try to detect wrong order of events, but again allow for a bit of slack,
			// as some drivers (this time on PowerPC) have their own share of trouble. Specifically,
			// this might happen if a driver performs VBL timestamping at the end of a VBL interval,
			// instead of at the beginning. In that case, the bufferswap may happen at rising-edge
			// of VBL, get acknowledged and timestamped by us somewhere in the middle of VBL, but
			// the postflip timestamping via IRQ may carry a timestamp at end of VBL.
			// ==> Swap would have happened correctly within VBL and postflip timestamp would
			// be valid, just the order would be unexpected. We set a slack of 2 msecs, because
			// the duration of a VBL interval is usually no longer than that.
			if (postflip_vbltimestamp - time_at_swapcompletion > 0.002) {
				// VBL irq queries broken! Disable them.
				if (verbosity > 0) {
					printf("PTB-ERROR: VBL kernel-level timestamp queries broken on your setup [Impossible order of events]!\n");
					printf("PTB-ERROR: Will disable them for now until the problem is resolved. You may want to restart Matlab and retry.\n");
					printf("PTB-ERROR: postflip - time_at_swapcompletion == %lf secs.\n", postflip_vbltimestamp - time_at_swapcompletion);
					printf("PTB-ERROR: Btw. if you are running in windowed mode, this is not unusual -- timestamping doesn't work well in windowed mode...\n");
				}
				
				PsychPrefStateSet_VBLTimestampingMode(-1);
				time_at_vbl = time_at_swapcompletion;
				*time_at_onset=time_at_vbl;
			}			
		}
		
        // Check for missed / skipped frames: We exclude the very first "Flip" after
        // creation of the onscreen window from the check, as deadline-miss is expected
        // in that case:
        if ((time_at_vbl > tshouldflip) && (windowRecord->time_at_last_vbl!=0)) {
            // Deadline missed!
            windowRecord->nr_missed_deadlines = windowRecord->nr_missed_deadlines + 1;
        }
        
        // Return some estimate of how much we've missed our deadline (positive value) or
        // how much headroom was left (negative value):
        *miss_estimate = time_at_vbl - tshouldflip;
        
        // Update timestamp of last vbl:
        windowRecord->time_at_last_vbl = time_at_vbl;
    }
    else {
        // syncing to vbl is disabled, time_at_vbl becomes meaningless, so we set it to a
        // safe default of zero to indicate this.
        time_at_vbl = 0;
        *time_at_onset = 0;
        *beamPosAtFlip = -1;  // Ditto for beam position...
        
        // Invalidate timestamp of last vbl:
        windowRecord->time_at_last_vbl = 0;
    }
    
    // The remaining code will run asynchronously on the GPU again and prepares the back-buffer
    // for drawing of next stim.
    PsychPostFlipOperations(windowRecord, dont_clear);
        
    // Part 2 of workaround- /checkcode for syncing to vertical retrace:
    if (vblsyncworkaround) {
        glReadBuffer(GL_FRONT);
        glPixelStorei(GL_PACK_ALIGNMENT,1);
        glReadPixels(0,0,1,1,GL_RED,GL_UNSIGNED_BYTE, &bufferstamp);
        if (bufferstamp!=id) {
            printf("%i -> %i  ", id, bufferstamp);
            glColor3b((GLint) bufferstamp, (GLint) 0,(GLint) 0);
            glBegin(GL_TRIANGLES);
            glVertex2d(20,20);
            glVertex2d(200,200);
            glVertex2d(20,200);
            glEnd();
        }

        id++;
    }

	// Special imaging mode active? in that case we need to restore drawing engine state to preflip state.
	if (windowRecord->imagingMode > 0) {
		PsychSetDrawingTarget(windowRecord);
	}

    // Restore assignments of read- and drawbuffers to pre-Flip state:
    glReadBuffer(read_buffer);
    glDrawBuffer(draw_buffer);

    // Reset flags used for avoiding redundant Pipeline flushes and backbuffer-backups:
    // This flags are altered and checked by SCREENDrawingFinished() and PsychPreFlipOperations() as well:
    windowRecord->PipelineFlushDone = false;
    windowRecord->backBufferBackupDone = false;
    
    // If we disabled (upon request) VBL syncing, we have to reenable it here:
    if (vbl_synclevel==2 || (windowRecord->inRedTable && (PSYCH_SYSTEM == PSYCH_WINDOWS))) {
		PsychOSSetVBLSyncLevel(windowRecord, 1);
		// Reenable also for a slave window, if any:
		if (windowRecord->slaveWindow) PsychOSSetVBLSyncLevel(windowRecord->slaveWindow, 1);
    }
    
    // Was this an experimental Multiflip with "hard" busy flipping?
    if (multiflip==2) {
        // Reenable VBL-Sync for all onscreen windows except our primary one:
        for(i=0;i<numWindows;i++) {
            if (PsychIsOnscreenWindow(windowRecordArray[i]) && (windowRecordArray[i]!=windowRecord)) {
				PsychOSSetVBLSyncLevel(windowRecordArray[i], 1);
            }
        }
    }
    
    if (multiflip>0) {
        // Cleanup our multiflip windowlist: Not thread-safe!
        PsychDestroyVolatileWindowRecordPointerList(windowRecordArray);
    }
    
	 // Cleanup temporary gamma tables if needed: Should be thread-safe due to standard libc call.
	 if ((windowRecord->inRedTable) && (windowRecord->loadGammaTableOnNextFlip > 0)) {
		free(windowRecord->inRedTable); windowRecord->inRedTable = NULL;
		free(windowRecord->inGreenTable); windowRecord->inGreenTable = NULL;
		free(windowRecord->inBlueTable); windowRecord->inBlueTable = NULL;
		windowRecord->inTableSize = 0;
		windowRecord->loadGammaTableOnNextFlip = 0;
	 }

    // We take a second timestamp here to mark the end of the Flip-routine and return it to "userspace"
    PsychGetAdjustedPrecisionTimerSeconds(time_at_flipend);
    
    // Done. Return high resolution system time in seconds when VBL happened.
    return(time_at_vbl);
}

/*
    PsychSetGLContext()
    
    Set the window to which GL drawing commands are sent.  
*/
void PsychSetGLContext(PsychWindowRecordType *windowRecord)
{
	PsychWindowRecordType *parentRecord;
	
	// Child protection: Calling this function is only allowed in non-userspace rendering mode:
    if (PsychIsUserspaceRendering()) PsychErrorExitMsg(PsychError_user, "You tried to call a Screen graphics command after Screen('BeginOpenGL'), but without calling Screen('EndOpenGL') beforehand! Read the help for 'Screen EndOpenGL?'.");

	// Check if any async flip on any onscreen window in progress: In that case only the async flip worker thread is allowed to call PsychSetGLContext()
	// on async-flipping onscreen windows, and none of the threads is allowed to attach to non-onscreen-window ressources.
	// asyncFlipOpsActive is a count of currently async-flipping onscreen windows...
	#if PSYCH_SYSTEM != PSYCH_WINDOWS
		// Any async flips in progress? If not, then we can skip this whole checking...
		if (asyncFlipOpsActive > 0) {
			// At least one async flip in progress. Find the parent window of this windowRecord, ie.,
			// the onscreen window which "owns" the relevant OpenGL context. This can be the windowRecord
			// itself if it is an onscreen window:
			parentRecord = PsychGetParentWindow(windowRecord);
			
			// We allow the main thread to attach to the OpenGL contexts owned by "parentRecord" windows which are not (yet) involved in an async flip operation.
			// Only the worker thread of an async flipping "parentRecord" window is allowed to attach that window while async flip in progress:
			if ((parentRecord->flipInfo) && (parentRecord->flipInfo->asyncstate == 1) && (!pthread_equal(parentRecord->flipInfo->flipperThread, pthread_self()))) {
				// Wrong thread - This one is not allowed to attach to any OpenGL context for this parentRecord at the moment.
				// Likely a programming error by the user:
				if (!PsychIsOnscreenWindow(windowRecord)) {
					PsychErrorExitMsg(PsychError_user, "Your code tried to execute a Screen() graphics command or Matlab/Octave/C OpenGL command for an offscreen window, texture or proxy while some asynchronous flip operation was in progress for the parent window!\nThis is not allowed for that command! Finalize the async flip(s) first via Screen('AsyncFlipCheckEnd') or Screen('AsyncFlipEnd')!");
				}
				else {
					PsychErrorExitMsg(PsychError_user, "Your code tried to execute a Screen() graphics command or Matlab/Octave/C OpenGL command for an onscreen window while an asynchronous flip operation was in progress on that window!\nThis is not allowed for that command! Finalize the flip first via Screen('AsyncFlipCheckEnd') or Screen('AsyncFlipEnd')!");    
				}
			}
			
			// Note: We allow drawing to non-async-flipping onscreen windows and offscreen windows/textures/proxies which don't have
			// the same OpenGL context as any of the async-flipping windows. This should be sufficient to prevent crash'es and/or
			// other GL malfunctions, so formally this is safe. However, once created all textures/FBO's are shared between all contexts
			// because we enable context ressource sharing on all our contexts. That means that operations performed on them may impact
			// the performance and latency of operations in unrelated contexts, e.g., the ones involved in async flip --> Potential to
			// impair the stimulus onset timing of async-flipping windows. Another reason for impaired timing is that some ressources on
			// the GPU can't be used in parallel to async flips, so operations in parallely executing contexts get serialized due to
			// ressource contention. Examples would be the command processor CP on ATI Radeons, which only exists once, so all command
			// streams have to be serialized --> Bufferswap trigger command packet could get stuck/stalled behind a large bunch of drawing
			// commands for an unrelated GL context and command stream in the worst case! This is totally dependent on the gfx-drivers
			// implementation of register programming for swap -- e.g., done through CP or done via direct register writes?
			//
			// All in all, the option to render to non-flipping ressources may give a performance boost when used very carefully, but
			// may also impair good timing if not used by advanced/experienced users. But in some sense that is true for the whole async
			// flip stuff -- its pushing the system pretty hard so it will always be more fragile wrt. system load fluctuations etc...
		}
	#endif
	
	// Call OS - specific context switching code:
    PsychOSSetGLContext(windowRecord);
}

/*
    PsychClearGLContext()
    
    Clear the drawing context.  
*/
void PsychUnsetGLContext(void)
{
  PsychErrorExitMsg(PsychError_internal, "Ouuuucchhhh!!! PsychUnsetGLContext(void) called!!!!\n");    
}

/*
    PsychGetMonitorRefreshInterval() -- Monitor refresh calibration.
    
    When called with numSamples>0, will enter a calibration loop that measures until
    either numSamples valid samples have been taken *and* measured standard deviation
    is below some spec'd threshold, or some maximum time has elapsed (timeout).
    The refresh interval - intervalHint (if >0) is taken as a hint for the real refresh
    interval to stabilize measurements by detection/elimination of outliers.

    The estimate is stored in the windowRecord of the associated onscreen-window for
    use by different PTB-Routines, e.g., PsychFlipWindowBuffers(), it is also returned
    in the double-return argument.
 
    When called with numSamples=0, will return the refresh interval estimated by a
    previous calibration run of the routine.
 
    This routine will be called in PsychOpenOnscreenWindow for initial calibration,
    taking at least 50 samples and can be triggered by Matlab by calling the
    SCREENGetFlipInterval routine, if an experimenter needs a more accurate estimate...
*/
double PsychGetMonitorRefreshInterval(PsychWindowRecordType *windowRecord, int* numSamples, double* maxsecs, double* stddev, double intervalHint)
{
    int i, j;
    double told, tnew, tdur, tstart;
    double tstddev=10000.0f;
    double tavg=0;
    double tavgsq=0;
    double n=0;
    double reqstddev=*stddev;   // stddev contains the requested standard deviation.
    int fallthroughcount=0;
    double* samples = NULL;
	int maxlogsamples = 0;
	
    // Child protection: We only work on double-buffered onscreen-windows...
    if (windowRecord->windowType != kPsychDoubleBufferOnscreen) {
        PsychErrorExitMsg(PsychError_InvalidWindowRecord, "Tried to query/measure monitor refresh interval on a window that's not double-buffered and on-screen.");
    }
    
    // Calibration run requested?
    if (*numSamples>0) {
        // Calibration run of 'numSamples' requested. Let's do it.
        
		if (PsychPrefStateGet_Verbosity()>4) {
			// Allocate a sample logbuffer for maxsecs duration at 1000 hz refresh:
			maxlogsamples =  (int) (ceil(*maxsecs) * 1000);
			samples = calloc(sizeof(double), maxlogsamples);
		}
		
        // Switch to RT scheduling for timing tests:
        PsychRealtimePriority(true);

        // Wipe out old measurements:
        windowRecord->IFIRunningSum = 0;
        windowRecord->nrIFISamples = 0;

        // Enable this windowRecords framebuffer as current drawingtarget: Important to do this, even
		// if it gets immediately disabled below, as this also sets the OpenGL context and takes care
		// of all state transitions between onscreen/offscreen windows etc.:
        PsychSetDrawingTarget(windowRecord);

		// Disable any shaders:
		PsychSetShader(windowRecord, 0);
		
		// ...and immediately disable it in imagingmode, because it won't be the system backbuffer,
		// but a FBO -- which would break sync of glFinish() with bufferswaps and vertical retrace.
		if ((windowRecord->imagingMode > 0) && (windowRecord->imagingMode != kPsychNeedFastOffscreenWindows)) PsychSetDrawingTarget(NULL);
		
        glDrawBuffer(GL_BACK_LEFT);
        
        PsychGetAdjustedPrecisionTimerSeconds(&tnew);
        tstart = tnew;
		told = -1;
		
		// Schedule a buffer-swap on next VBL:
		PsychOSFlipWindowBuffers(windowRecord);
		
        // Take samples during consecutive refresh intervals:
        // We measure until either:
        // - A maximum measurment time of maxsecs seconds has elapsed... (This is the emergency switch to prevent infinite loops).
        // - Or at least numSamples valid samples have been taken AND measured standard deviation is below the requested deviation stddev.
        for (i=0; (fallthroughcount<10) && ((tnew - tstart) < *maxsecs) && (n < *numSamples || ((n >= *numSamples) && (tstddev > reqstddev))); i++) {
            // Schedule a buffer-swap on next VBL:
			PsychOSFlipWindowBuffers(windowRecord);
            
            // Wait for it, aka VBL start: See PsychFlipWindowBuffers for explanation...
            glBegin(GL_POINTS);
            glColor4f(0,0,0,0);
            glVertex2i(10,10);
            glEnd();
            
            // This glFinish() will wait until point drawing is finished, ergo backbuffer was ready
            // for drawing, ergo buffer swap in sync with start of VBL has happened.
            glFinish();
            
            // At this point, start of VBL has happened and we can continue execution...
            // We take our timestamp here:
            PsychGetAdjustedPrecisionTimerSeconds(&tnew);
            
            // We skip the first measurement, because we first need to establish an initial base-time 'told'
            if (told > 0) {
                // Compute duration of this refresh interval in tnew:
                tdur = tnew - told;
                
                // This is a catch for complete sync-failure:
                // tdur < 0.004 can happen occasionally due to operating system scheduling jitter,
                // in this case tdur will be >> 1 monitor refresh for next iteration. Both samples
                // will be rejected, so they don't skew the estimate.
                // But if tdur < 0.004 for multiple consecutive frames, this indicates that
                // synchronization fails completely and we are just "falling through" glFinish().
                // A fallthroughcount>=10 will therefore abort the measurment-loop and invalidate
                // the whole result - indicating VBL sync trouble...
                // We need this additional check, because without it, we could get 1 valid sample with
                // approx. 10 ms ifi accidentally because the whole loop can stall for 10 ms on overload.
                // The 10 ms value is a valid value corresponding to 100 Hz refresh and by coincidence its
                // the "standard-timeslicing quantum" of the MacOS-X scheduler... ...Wonderful world of
                // operating system design and unintended side-effects for poor psychologists... ;-)
                fallthroughcount = (tdur < 0.004) ? fallthroughcount+1 : 0;

                // We accept the measurement as valid if either no intervalHint is available as reference or
                // we are in an interval between +/-20% of the hint.
                // We also check if interval corresponds to a measured refresh between 25 Hz and 250 Hz. Other
                // values are considered impossible and are therefore rejected...
                // If we are in OpenGL native stereo display mode, aka temporally interleaved flip-frame stereo,
                // then we also accept samples that are in a +/-20% rnage around twice the intervalHint. This is,
                // because in OpenGL stereo mode, ATI hardware doubles the flip-interval: It only flips every 2nd
                // video refresh, so a doubled flip interval is a legal valid result.
                if ((tdur >= 0.004 && tdur <= 0.040) && ((intervalHint<=0) || (intervalHint>0 &&
                    ( ((tdur > 0.8 * intervalHint) && (tdur < 1.2 * intervalHint)) ||
                      (((windowRecord->stereomode==kPsychOpenGLStereo) || (windowRecord->multiSample > 0)) && (tdur > 0.8 * 2 * intervalHint) && (tdur < 1.2 * 2 * intervalHint))
                    )))) {
                    // Valid measurement - Update our estimate:
                    windowRecord->IFIRunningSum = windowRecord->IFIRunningSum + tdur;
                    windowRecord->nrIFISamples = windowRecord->nrIFISamples + 1;

                    // Update our sliding mean and standard-deviation:
                    tavg = tavg + tdur;
                    tavgsq = tavgsq + (tdur * tdur);
                    n=windowRecord->nrIFISamples;
                    tstddev = (n>1) ? sqrt( ( tavgsq - ( tavg * tavg / n ) ) / (n-1) ) : 10000.0f;

					// Update reference timestamp:
					told = tnew;
					
					// Pause for 2 msecs after a valid sample was taken. This to guarantee we're out
					// of the VBL period of the successfull swap.
					PsychWaitIntervalSeconds(0.002);
                }
				else {
					// Rejected sample: Better invalidate told as well:
					//told = -1;
					// MK: Ok, i have no clue why above told = -1 is wrong, but doing it makes OS/X 10.4.10 much
					// more prone to sync failures, whereas not doing it makes it more reliable. Doesn't make
					// sense, but we are better off reverting to the old strategy...
					// Update: I think i know why. Some (buggy!) drivers, e.g., the ATI Radeon X1600 driver on
					// OS/X 10.4.10, do not limit the number of bufferswaps to 1 per refresh cycle as mandated
					// by the spec, but they allow as many bufferswaps as you want, as long as all of them happen
					// inside the VBL period! Basically the swap-trigger seems to be level-triggered instead of
					// edge-triggered. This leads to a ratio of 2 invalid samples followed by 1 valid sample.
					// If we'd reset our told at each invalid sample, we would need over 3 times the amount of
					// samples for a useable calibration --> No go. Now we wait for 2 msecs after each successfull
					// sample (see above), so the VBL period will be over before we manage to try to swap again.
					
					// Reinitialize told to tnew, otherwise errors can accumulate:
					told = tnew;

					// Pause for 2 msecs after a valid sample was taken. This to guarantee we're out
					// of the VBL period of the successfull swap.
					PsychWaitIntervalSeconds(0.002);
				}
				
				// Store current sample in samplebuffer if requested:
				if (samples && i < maxlogsamples) samples[i] = tdur;
            }
			else {
				// (Re-)initialize reference timestamp:
				told = tnew;

				// Pause for 2 msecs after a first sample was taken. This to guarantee we're out
				// of the VBL period of the successfull swap.
				PsychWaitIntervalSeconds(0.002);
			}
			
        } // Next measurement loop iteration...
        
        // Switch back to old scheduling after timing tests:
        PsychRealtimePriority(false);
        
        // Ok, now we should have a pretty good estimate of IFI.
        if ( windowRecord->nrIFISamples <= 0 ) {
            printf("PTB-WARNING: Couldn't even collect one single valid flip interval sample! Sanity range checks failed!\n");
            printf("PTB-WARNING: Could be a system bug, or a temporary timing problem. Retrying the procedure might help if\n");
            printf("PTB-WARNING: the latter is the culprit.\n");
        }

        // Some additional check:
        if (fallthroughcount>=10) {
            // Complete sync failure! Invalidate all measurements:
            windowRecord->nrIFISamples = 0;
            n=0;
            tstddev=1000000.0;
            windowRecord->VideoRefreshInterval = 0;
            printf("PTB-WARNING: Couldn't collect valid flip interval samples! Fatal VBL sync failure!\n");
            printf("PTB-WARNING: Either synchronization of doublebuffer swapping to the vertical retrace signal of your display is broken,\n");
            printf("PTB-WARNING: or the mechanism for detection of swap completion is broken. In any case, this is an operating system or gfx-driver bug!\n");
        }
        
        *numSamples = n;
        *stddev = tstddev;
		
		// Verbose output requested? We dump our whole buffer of samples to the console:
		if (samples) {
			printf("\n\nPTB-DEBUG: Output of all acquired samples of calibration run follows:\n");
			for (j=0; j<i; j++) printf("PTB-DEBUG: Sample %i: %lf\n", j, samples[j]);
			printf("PTB-DEBUG: End of calibration data for this run...\n\n");
			free(samples);
			samples = NULL;
		}
		
    } // End of IFI measurement code.
    else {
        // No measurements taken...
        *numSamples = 0;
        *stddev = 0;
    }
    
    // Return the current estimate of flip interval & monitor refresh interval, if any...
    if (windowRecord->nrIFISamples > 0) {
        return(windowRecord->IFIRunningSum / windowRecord->nrIFISamples);
    }
    else {
        // Invalidate refresh on error.
        windowRecord->VideoRefreshInterval = 0;
        return(0);
    }
}

/*
    PsychVisualBell()
 
    Visual bell: Flashes the screen multiple times by changing background-color.
    This meant to be used when PTB detects some condition important for the user.
    The idea is to output some debug/warning messages to the Matlab command window,
    but as the user can't see them while the fullscreen stimulus window is open, we
    have to tell him/her somehow that his attention is recquired.
    This is mostly used in Screen('OpenWindow') of some tests fail or similar things...
 
    "duration" Defines duration in seconds.
    "belltype" Defines kind of info (Info = 0, Warning = 1, Error/Urgent = 2, 3 = Visual flicker test-sheet)
 
*/
void PsychVisualBell(PsychWindowRecordType *windowRecord, double duration, int belltype)
{
    double tdeadline, tcurrent, v=0;
    GLdouble color[4];
    int f=0;
    int scanline;
    CGDirectDisplayID cgDisplayID;
    float w,h;
    int visual_debuglevel;
    PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, windowRecord->screenNumber);    
    
    // Query current visual feedback level and abort, if it doesn't
    // allow requested type of visual feedback:
    visual_debuglevel = PsychPrefStateGet_VisualDebugLevel();    
    if (belltype == 0 && visual_debuglevel < 3) return;
    if (belltype == 1 && visual_debuglevel < 2) return;
    if (belltype == 2 && visual_debuglevel < 1) return;
    if (belltype == 3 && visual_debuglevel < 5) return;
    
    glGetDoublev(GL_COLOR_CLEAR_VALUE, (GLdouble*) &color);

    PsychGetAdjustedPrecisionTimerSeconds(&tdeadline);
    tdeadline+=duration;
    
    // Enable this windowRecords framebuffer as current drawingtarget:
    PsychSetDrawingTarget(windowRecord);

    w=PsychGetWidthFromRect(windowRecord->rect);
    h=PsychGetHeightFromRect(windowRecord->rect);
    
    // Clear out both buffers so it doesn't lool like junk:
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    PsychOSFlipWindowBuffers(windowRecord);
    glClear(GL_COLOR_BUFFER_BIT);
    PsychOSFlipWindowBuffers(windowRecord);
    
    if (belltype==3) {
        // Test-Sheet mode: Need smaller warning triangle...
        w=w/3;
        h=h/3;
    }
    
    do {
        // Take timestamp for abort-check and driving animation:
        PsychGetAdjustedPrecisionTimerSeconds(&tcurrent);

        // Calc our visual ;-)
        v=0.5 + 0.5 * sin(tcurrent*6.283);
        
        switch (belltype) {
            case 0: // Info - Make it blue
                glClearColor(0,0,v,0);
                break;
            case 1: // Warning - Make it yellow
                glClearColor(v,v,0,0);
                break;
            case 2: // Error - Make it red
                glClearColor(v,0,0,0);
            break;
            case 3: // Test-Sheet - Don't clear...
                // Draw some flickering area (alternating black-white flicker)
                f=1-f;
                glBegin(GL_QUADS);
                glColor3f(f,f,f);
                glVertex2f(0.00*w, 0.00*h);
                glVertex2f(2.00*w, 0.00*h);
                glVertex2f(2.00*w, 3.00*h);
                glVertex2f(0.00*w, 3.00*h);
                glColor3f(0,0,v);
                glVertex2f(0.00*w, 0.00*h);
                glVertex2f(1.00*w, 0.00*h);
                glVertex2f(1.00*w, 1.00*h);
                glVertex2f(0.00*w, 1.00*h);
                glEnd();
            break;
        }
        
        if (belltype!=3) glClear(GL_COLOR_BUFFER_BIT);
        
        // Draw a yellow triangle with black border:
        glColor3f(0,0,0);
        glBegin(GL_TRIANGLES);
        glVertex2f(0.1*w, 0.1*h);
        glVertex2f(0.9*w, 0.1*h);
        glVertex2f(0.5*w, 0.9*h);
        glColor3f(1,1,0);
        glVertex2f(0.2*w, 0.2*h);
        glVertex2f(0.8*w, 0.2*h);
        glVertex2f(0.5*w, 0.8*h);
        glEnd();
        // Draw a black exclamation mark into triangle:
        glBegin(GL_QUADS);
        glColor3f(0,0,0);
        glVertex2f(0.47*w, 0.23*h);
        glVertex2f(0.53*w, 0.23*h);
        glVertex2f(0.53*w, 0.55*h);
        glVertex2f(0.47*w, 0.55*h);
        glVertex2f(0.47*w, 0.60*h);
        glVertex2f(0.53*w, 0.60*h);
        glVertex2f(0.53*w, 0.70*h);
        glVertex2f(0.47*w, 0.70*h);
        glEnd();
        
        // Initiate back-front buffer flip:
	PsychOSFlipWindowBuffers(windowRecord);
        
        // Our old VBL-Sync trick again... We need sync to VBL to visually check if
        // beamposition is locked to VBL:
        // We draw our single pixel with an alpha-value of zero - so effectively it doesn't
        // change the color buffer - just the z-buffer if z-writes are enabled...
        glColor4f(0,0,0,0);
        glBegin(GL_POINTS);
        glVertex2i(10,10);
        glEnd();        
        // This glFinish() will wait until point drawing is finished, ergo backbuffer was ready
        // for drawing, ergo buffer swap in sync with start of VBL has happened.
        glFinish();

        // Query and visualize scanline immediately after VBL onset, aka return of glFinish();
        scanline=(int) PsychGetDisplayBeamPosition(cgDisplayID, windowRecord->screenNumber);    
        if (belltype==3) {
            glColor3f(1,1,0);
            glBegin(GL_LINES);
            glVertex2f(2*w, scanline);
            glVertex2f(3*w, scanline);
            glEnd();
        }
    } while (tcurrent < tdeadline);

    // Restore clear color:
    glClearColor(color[0], color[1], color[2], color[3]);

    return;
}

/*
 * PsychPreFlipOperations()  -- Prepare windows backbuffer for flip.
 *
 * This routine performs all preparatory work to bring the windows backbuffer in its
 * final state for bufferswap as soon as possible.
 *
 * If a special stereo display mode is active, it performs all necessary drawing/setup/
 * compositing operations to assemble the final stereo display from the content of diverse
 * stereo backbuffers/AUX buffers/stereo metadata and such.
 *
 * If clearmode = Don't clear after flip is selected, the necessary backup copy of the
 * backbuffers into AUX buffers is made, so backbuffer can be restored to previous state
 * after Flip.
 *
 * This routine is called automatically by PsychFlipWindowBuffers on Screen('Flip') time as
 * well as by Screen('DrawingFinished') for manually triggered preflip work.
 *
 * -> Unifies the code in Flip and DrawingFinished.
 *
 */
void PsychPreFlipOperations(PsychWindowRecordType *windowRecord, int clearmode)
{
    int screenwidth=(int) PsychGetWidthFromRect(windowRecord->rect);
    int screenheight=(int) PsychGetHeightFromRect(windowRecord->rect);
    int stereo_mode=windowRecord->stereomode;
	int imagingMode = windowRecord->imagingMode;
	int viewid, hookchainid;
	GLint read_buffer, draw_buffer, blending_on;
    GLint auxbuffers;

    // Early reject: If this flag is set, then there's no need for any processing:
    // We only continue processing textures, aka offscreen windows...
    if (windowRecord->windowType!=kPsychTexture && windowRecord->backBufferBackupDone) return;

    // Enable this windowRecords framebuffer as current drawingtarget:
    PsychSetDrawingTarget(windowRecord);
    
    // We stop processing here if window is a texture, aka offscreen window...
    if (windowRecord->windowType==kPsychTexture) return;
    
	// Disable any shaders:
	PsychSetShader(windowRecord, 0);
	
    // Reset viewport to full-screen default:
    glViewport(0, 0, screenwidth, screenheight);
    glScissor(0, 0, screenwidth, screenheight);
    
    // Reset color buffer writemask to "All enabled":
    glColorMask(TRUE, TRUE, TRUE, TRUE);

    // Query number of available AUX-buffers:
    glGetIntegerv(GL_AUX_BUFFERS, &auxbuffers);

    // Set transform matrix to well-defined state:
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    
	// The following code is for traditional non-imaging rendering. Its also executed for
	// the special case of FBO backed Offscreen windows only:
	if (imagingMode == 0 || imagingMode == kPsychNeedFastOffscreenWindows) {
		// Check for compressed stereo handling...
		if (stereo_mode==kPsychCompressedTLBRStereo || stereo_mode==kPsychCompressedTRBLStereo) {
			if (auxbuffers<2) {
				PsychErrorExitMsg(PsychError_user, "OpenGL AUX buffers unavailable! The requested stereo mode doesn't work without them.\n"
								  "Either unsupported by your graphics card, or you disabled them via call to Screen('Preference', 'ConserveVRAM')?");
			}
			
			// Compressed stereo mode active. Compositing already done?
			// Backup backbuffer -> AUX buffer: We trigger this via transition to "no buffer":
			PsychSwitchCompressedStereoDrawBuffer(windowRecord, 2);
			
			// Ok, now both AUX buffers contain the final stereo content: Compose them into
			// back-buffer:
			PsychComposeCompressedStereoBuffer(windowRecord);
		}
		// Non-compressed stereo case: Mono or other stereo alg. Normal treatment applies...
		// Check if we should do the backbuffer -> AUX buffer backup, because we use
		// clearmode 1 aka "Don't clear after flip, but retain backbuffer content"
		else if (clearmode==1 && windowRecord->windowType==kPsychDoubleBufferOnscreen) {
			// Backup current assignment of read- writebuffers:
			glGetIntegerv(GL_READ_BUFFER, &read_buffer);
			glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
			blending_on = (int) glIsEnabled(GL_BLEND);
			glDisable(GL_BLEND);
			
			// Is this window equipped with a native OpenGL stereo rendering context?
			// If so, then we need to backup both backbuffers (left-eye and right-eye),
			// instead of only the monoscopic one.
			if (stereo_mode==kPsychOpenGLStereo) {
				if (auxbuffers<2) {
					PsychErrorExitMsg(PsychError_user, "OpenGL AUX buffers unavailable! dontclear=1 in Screen-Flip doesn't work without them.\n"
									  "Either unsupported by your graphics card, or you disabled them via call to Screen('Preference', 'ConserveVRAM')?");
				}
				
				glDrawBuffer(GL_AUX0);
				glReadBuffer(GL_BACK_LEFT);
				glRasterPos2i(0, screenheight);
				glCopyPixels(0, 0, screenwidth, screenheight, GL_COLOR);            
				glDrawBuffer(GL_AUX1);
				glReadBuffer(GL_BACK_RIGHT);
				glRasterPos2i(0, screenheight);
				glCopyPixels(0, 0, screenwidth, screenheight, GL_COLOR);            
			}
			else {
				if (auxbuffers<1) {
					PsychErrorExitMsg(PsychError_user, "OpenGL AUX buffers unavailable! dontclear=1 in Screen-Flip doesn't work without them.\n"
									  "Either unsupported by your graphics card, or you disabled them via call to Screen('Preference', 'ConserveVRAM')?");
				}
				glDrawBuffer(GL_AUX0);
				glReadBuffer(GL_BACK);
				glRasterPos2i(0, screenheight);
				glCopyPixels(0, 0, screenwidth, screenheight, GL_COLOR);            
			}
			
			if (blending_on) glEnable(GL_BLEND);
			
			// Restore assignment of read- writebuffers:
			glReadBuffer(read_buffer);
			glDrawBuffer(draw_buffer);        
		}

		// Check if the finalizer blit chain is operational. This is the only blit chain available for preflip operations in non-imaging mode,
		// useful for encoding special information into final framebuffer images, e.g., sync lines, time stamps, cluts...
		// All other blit chains are only available in imaging mode - they need support for shaders and framebuffer objects...
		if (PsychIsHookChainOperational(windowRecord, kPsychLeftFinalizerBlit) || PsychIsHookChainOperational(windowRecord, kPsychRightFinalizerBlit)) {
			glGetIntegerv(GL_READ_BUFFER, &read_buffer);
			glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
			blending_on = (int) glIsEnabled(GL_BLEND);
			glDisable(GL_BLEND);
			
			// Process each of the (up to two) streams:
			for (viewid = 0; viewid < ((stereo_mode == kPsychOpenGLStereo) ? 2 : 1); viewid++) {
				
				// Select drawbuffer:
				if (stereo_mode == kPsychOpenGLStereo) {
					// Quad buffered stereo: Select proper backbuffer:
					glDrawBuffer((viewid==0) ? GL_BACK_LEFT : GL_BACK_RIGHT);
				} else {
					// Mono mode: Select backbuffer:
					glDrawBuffer(GL_BACK);
				}
				
				// This special purpose blit chains can be used to encode low-level information about frames into
				// the frames or do other limited per-frame processing. Their main use (as of now) is to draw
				// the blue-line sync signal into quad-buffered windows in quad-buffered stereo mode. One could
				// use them e.g., to encode a frame index, a timestamp or a trigger signal into frames as well.
				// Encoding CLUTs for devices like the Bits++ is conceivable as well - these would be automatically
				// synchronous to frame updates and could be injected from our own gamma-table functions.
				PsychPipelineExecuteHook(windowRecord, (viewid==0) ? kPsychLeftFinalizerBlit : kPsychRightFinalizerBlit, NULL, NULL, TRUE, FALSE, NULL, NULL, NULL, NULL);				
			}
			
			// Restore blending mode:
			if (blending_on) glEnable(GL_BLEND);
			
			// Restore assignment of read- writebuffers:
			glReadBuffer(read_buffer);
			glDrawBuffer(draw_buffer);
		}
		
		// Restore modelview matrix:
		glPopMatrix();

	}	// End of traditional preflip path.
	
	if (imagingMode && imagingMode!=kPsychNeedFastOffscreenWindows) {
		// Preflip operations for imaging mode:

		// Detach any active drawing targets:
		PsychSetDrawingTarget(NULL);

		// Reset modelview matrix to identity:
		glLoadIdentity();

		// Save all state:
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		
		// Disable alpha-blending:
		glDisable(GL_BLEND);
		
		// Execute post processing sequence for this onscreen window:
		
		// Is there a need for special processing on the drawBufferFBO during copy to inputBufferFBO?
		// Or are both identical?
		for (viewid = 0; viewid < ((stereo_mode > 0) ? 2 : 1); viewid++) {
			if (windowRecord->inputBufferFBO[viewid] != windowRecord->drawBufferFBO[viewid]) {
				// Separate draw- and inputbuffers: We need to copy the drawBufferFBO to its
				// corresponding inputBufferFBO, applying a special conversion operation.
				// We use this for multisample-resolve of multisampled drawBufferFBO's.
				// A simple glBlitFramebufferEXT() call will do the copy & downsample operation:
				glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, windowRecord->fboTable[windowRecord->drawBufferFBO[viewid]]->fboid);
				glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, windowRecord->fboTable[windowRecord->inputBufferFBO[viewid]]->fboid);
				glBlitFramebufferEXT(0, 0, windowRecord->fboTable[windowRecord->inputBufferFBO[viewid]]->width, windowRecord->fboTable[windowRecord->inputBufferFBO[viewid]]->height,
									 0, 0, windowRecord->fboTable[windowRecord->inputBufferFBO[viewid]]->width, windowRecord->fboTable[windowRecord->inputBufferFBO[viewid]]->height,
									 GL_COLOR_BUFFER_BIT, GL_NEAREST);
			}
		}
		
		// Reset framebuffer binding to something safe - The system framebuffer:
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		
		// Generic image processing on viewchannels enabled?
		if (imagingMode & kPsychNeedImageProcessing) {
			// Yes. Process each of the (up to two) streams:
			for (viewid = 0; viewid < ((stereo_mode > 0) ? 2 : 1); viewid++) {
				// Processing chain enabled and non-empty?
				hookchainid = (viewid==0) ? kPsychStereoLeftCompositingBlit : kPsychStereoRightCompositingBlit;
				if (PsychIsHookChainOperational(windowRecord, hookchainid)) {
					// Hook chain ready to do its job: Execute it.      userd,blitf
					// Don't supply user-specific data, blitfunction is default blitter, unless defined otherwise in blitchain,
					// srcfbos are read-only, swizzling forbidden, 2nd srcfbo doesn't exist (only needed for stereo merge op),
					// We provide a bounce-buffer... We could bind the 2nd channel in steromode if we wanted. Should we?
					// TODO: Define special userdata struct, e.g., for C-Callbacks or scripting callbacks?
					PsychPipelineExecuteHook(windowRecord, hookchainid, NULL, NULL, TRUE, FALSE, &(windowRecord->fboTable[windowRecord->inputBufferFBO[viewid]]), NULL, &(windowRecord->fboTable[windowRecord->processedDrawBufferFBO[viewid]]),  (windowRecord->processedDrawBufferFBO[2]>=0) ? &(windowRecord->fboTable[windowRecord->processedDrawBufferFBO[2]]) : NULL);
				}
				else {
					// Hook chain disabled by userspace or doesn't contain any instructions.
					// Execute our special identity blit chain to transfer the data from source buffer
					// to destination buffer:
					PsychPipelineExecuteHook(windowRecord, kPsychIdentityBlit, NULL, NULL, TRUE, FALSE, &(windowRecord->fboTable[windowRecord->inputBufferFBO[viewid]]), NULL, &(windowRecord->fboTable[windowRecord->processedDrawBufferFBO[viewid]]), NULL);
				}
			}
		}
		
		// At this point, processedDrawBufferFBO[0 and 1] contain the per-viewchannel result of
		// user defined (or stereo) image processing.
		
		// Stereo processing: This depends on selected stereomode...
		if (stereo_mode <= kPsychOpenGLStereo || stereo_mode == kPsychDualWindowStereo) {
			// No stereo or quad-buffered stereo or dual-window stereo - Nothing to do in merge stage.
		}
		else if (stereo_mode <= kPsychAnaglyphBRStereo) {
			// Merged stereo - All work is done by the anaglyph shader that was created for this purpose
			// in pipeline setup, no geometric transform or such are needed, so we can use the default blitter:
			if (PsychIsHookChainOperational(windowRecord, kPsychStereoCompositingBlit)) {
				// Don't supply user-specific data, blitfunction is default blitter, unless defined otherwise in blitchain,
				// srcfbos are read-only, swizzling forbidden, 2nd srcfbo is right-eye channel, whereas 1st srcfbo is left-eye channel.
				// We provide a bounce-buffer as well.
				// TODO: Define special userdata struct, e.g., for C-Callbacks or scripting callbacks?
				PsychPipelineExecuteHook(windowRecord, kPsychStereoCompositingBlit, NULL, NULL, TRUE, FALSE, &(windowRecord->fboTable[windowRecord->processedDrawBufferFBO[0]]), &(windowRecord->fboTable[windowRecord->processedDrawBufferFBO[1]]), &(windowRecord->fboTable[windowRecord->preConversionFBO[0]]), (windowRecord->preConversionFBO[2]>=0) ? &(windowRecord->fboTable[windowRecord->preConversionFBO[2]]) : NULL);
			}
			else {
				// Hook chain disabled by userspace or doesn't contain any instructions.
				// We vitally need the compositing chain, there's no simple fallback here!
				PsychErrorExitMsg(PsychError_internal, "Processing chain for stereo processing merge operations is needed, but empty or disabled - No visual output produced! Bug?!?\n");
			}			
		}
		else {
			// Invalid stereo mode?
			PsychErrorExitMsg(PsychError_internal, "Invalid stereo mode encountered!?!");
		}
		
		// At this point we have image data ready for final post-processing and special device output formatting...
		// In mono mode: Image in preConversionFBO[0].
		// In quad-buffered stereo mode: Left eye image in preConversionFBO[0], Right eye image in preConversionFBO[1].
		// In other stereo modes: Merged image in both preConversionFBO[0] and preConversionFBO[1], both reference the same image buffer.
		// If dual window output mode is requested, the merged - or single monoscopic - image is also in both
		// preConversionFBO[0] and preConversionFBO[1], as both reference the same image buffer.
		
		// Ready to create the final content, either for drawing into a snapshot buffer or into the real system framebuffer.
		// finalizedFBO[0] is set up to take the final image for anything but quad-buffered stereo.
		// In quad-buffered mode, finalizedFBO[0] shall receive the left-eye image, finalizedFBO[1] shall receive the right-eye image.
		// Each FBO is either a real FBO for framebuffer "screenshots" or the system framebuffer for final output into the backbuffer.

		// Process each of the (up to two) streams:
		for (viewid = 0; viewid < ((stereo_mode == kPsychOpenGLStereo || stereo_mode == kPsychDualWindowStereo || (imagingMode & kPsychNeedDualWindowOutput)) ? 2 : 1); viewid++) {

			// Select final drawbuffer if our target is the system framebuffer:
			if (windowRecord->fboTable[windowRecord->finalizedFBO[viewid]]->fboid == 0) {
				// Final target is system backbuffer:
				if (stereo_mode == kPsychOpenGLStereo) {
					// Quad buffered stereo: Select proper backbuffer:
					glDrawBuffer((viewid==0) ? GL_BACK_LEFT : GL_BACK_RIGHT);
				} else {
					// Mono mode: Select backbuffer:
					glDrawBuffer(GL_BACK);
				}
			}

			// Output conversion needed, processing chain(s) enabled and non-empty?
			if ((imagingMode & kPsychNeedOutputConversion) && (PsychIsHookChainOperational(windowRecord, kPsychFinalOutputFormattingBlit) ||
				(PsychIsHookChainOperational(windowRecord, kPsychFinalOutputFormattingBlit0) && PsychIsHookChainOperational(windowRecord, kPsychFinalOutputFormattingBlit1)))) {
				// Output conversion needed and unified chain or dual-channel chains operational.
				// Which ones to use?
				if (PsychIsHookChainOperational(windowRecord, kPsychFinalOutputFormattingBlit0)) {
					// Dual stream chains for separate formatting of both output views are active.
					// Unified chain active as well? That would be reason for a little warning about conflicts...
					if (PsychIsHookChainOperational(windowRecord, kPsychFinalOutputFormattingBlit) && (PsychPrefStateGet_Verbosity() > 1)) {
						printf("PTB-WARNING: Both, separate chains *and* unified chain for image output formatting active! Coding bug?!? Will use separate chains as override.\n");
					}

					// Use proper per view output formatting chain:
					PsychPipelineExecuteHook(windowRecord, ((viewid > 0) ? kPsychFinalOutputFormattingBlit1 : kPsychFinalOutputFormattingBlit0), NULL, NULL, TRUE, FALSE, &(windowRecord->fboTable[windowRecord->preConversionFBO[viewid]]), NULL, &(windowRecord->fboTable[windowRecord->finalizedFBO[viewid]]), (windowRecord->preConversionFBO[2]>=0) ? &(windowRecord->fboTable[windowRecord->preConversionFBO[2]]) : NULL);
				}
				else {
					// Single unified formatting chain to be used:
					PsychPipelineExecuteHook(windowRecord, kPsychFinalOutputFormattingBlit, NULL, NULL, TRUE, FALSE, &(windowRecord->fboTable[windowRecord->preConversionFBO[viewid]]), NULL, &(windowRecord->fboTable[windowRecord->finalizedFBO[viewid]]), (windowRecord->preConversionFBO[2]>=0) ? &(windowRecord->fboTable[windowRecord->preConversionFBO[2]]) : NULL);
				}
			}
			else {
				// No conversion needed or chain disabled: Do our identity blit, but only if really needed!
				// This gets skipped in mono-mode if no conversion needed and only single-pass image processing
				// applied. In that case, the image processing stage did the final blit already.
				if (windowRecord->preConversionFBO[viewid] != windowRecord->finalizedFBO[viewid]) {
					if ((imagingMode & kPsychNeedOutputConversion) && (PsychPrefStateGet_Verbosity()>3)) printf("PTB-INFO: Processing chain(s) for output conversion disabled -- Using identity copy as workaround.\n");
					PsychPipelineExecuteHook(windowRecord, kPsychIdentityBlit, NULL, NULL, TRUE, FALSE, &(windowRecord->fboTable[windowRecord->preConversionFBO[viewid]]), NULL, &(windowRecord->fboTable[windowRecord->finalizedFBO[viewid]]), NULL);				
				}
			}
			
			// This special purpose blit chains can be used to encode low-level information about frames into
			// the frames or do other limited per-frame processing. Their main use (as of now) is to draw
			// the blue-line sync signal into quad-buffered windows in quad-buffered stereo mode. One could
			// use them e.g., to encode a frame index, a timestamp or a trigger signal into frames as well.
			// Encoding CLUTs for devices like the Bits++ is conceivable as well - these would be automatically
			// synchronous to frame updates and could be injected from our own gamma-table functions.
			PsychPipelineExecuteHook(windowRecord, (viewid==0) ? kPsychLeftFinalizerBlit : kPsychRightFinalizerBlit, NULL, NULL, TRUE, FALSE, NULL, NULL, &(windowRecord->fboTable[windowRecord->finalizedFBO[viewid]]), NULL);				
		}
		
		// At this point we should have either a valid snapshot of the framebuffer in the finalizedFBOs, or
		// (the common case) the final image in the system backbuffers, ready for display after swap.
		
		// Disabled debug code:
		if (FALSE) {
			windowRecord->textureNumber = windowRecord->fboTable[windowRecord->drawBufferFBO[0]]->coltexid;
			
			// Now we need to blit the new rendertargets texture into the framebuffer. We need to make
			// sure that alpha-blending is disabled during this blit operation:
			// Alpha blending not enabled. Just blit it:
			PsychBlitTextureToDisplay(windowRecord, windowRecord, windowRecord->rect, windowRecord->rect, 0, 0, 1);
			
			windowRecord->textureNumber = 0;
		}

		// Restore all state, including blending and texturing state:
		glPopAttrib();
		
		// Restore modelview matrix:
		glPopMatrix();
		
		// In dual-window stereomode or dual-window output mode, we need to copy the finalizedFBO[1] into the backbuffer of
		// the slave-window:
		if (stereo_mode == kPsychDualWindowStereo || (imagingMode & kPsychNeedDualWindowOutput)) {
			if (windowRecord->slaveWindow == NULL) {
				if (PsychPrefStateGet_Verbosity()>3) printf("PTB-INFO: Skipping master->slave blit operation in dual-window stereo mode or output mode...\n");
			}
			else {
				// Perform blit operation: This looks weird. Due to the peculiar implementation of PsychPipelineExecuteHook() we must
				// pass slaveWindow as reference, so its GL context is activated. That means we will execute its default identity
				// blit chain (which was setup in SCREENOpenWindow.c). We blit from windowRecords finalizedFBO[1] - which is a color
				// texture with the final stimulus image for slaveWindow into finalizedFBO[0], which is just a pseudo-FBO representing
				// the system framebuffer - and therefore the backbuffer of slaveWindow.
				// -> This is a bit dirty and convoluted, but its the most efficient procedure for this special case.
				PsychPipelineExecuteHook(windowRecord->slaveWindow, kPsychIdentityBlit, NULL, NULL, TRUE, FALSE, &(windowRecord->fboTable[windowRecord->finalizedFBO[1]]), NULL, &(windowRecord->fboTable[windowRecord->finalizedFBO[0]]), NULL);				

				// Paranoia mode: A dual-window display configuration must swap both display windows in
				// close sync with each other and the vertical retraces of their respective display heads. Due
				// to the non-atomic submission of the swap-commands this config is especially prone to one display
				// missing the VBL deadline and flipping one video refresh too late. We try to reduce the chance of
				// this happening by forcing both rendering contexts of both displays to finish rendering now. That
				// way both backbuffers will be ready for swap and likelihood of a asymetric miss is much lower.
				// This may however cost a bit of performance on some setups...
				glFinish();
				
				// Restore current context and glFinish it as well:
				PsychSetGLContext(windowRecord);
				glFinish();
			}
		}
		
	}	// End of preflip operations for imaging mode:
    
    // Tell Flip that backbuffer backup has been done already to avoid redundant backups. This is a bit of a
	// unlucky name. It actually signals that all the preflip processing has been done, the old name is historical.
    windowRecord->backBufferBackupDone = true;

    return;
}

/*
 * PsychPostFlipOperations()  -- Prepare windows backbuffer after flip.
 *
 * This routine performs all preparatory work to bring the windows backbuffer in its
 * proper state for drawing the next stimulus after bufferswap has completed.
 *
 * If a special stereo display mode is active, it performs all necessary setup/
 * operations to restore the content of diverse stereo backbuffers/AUX buffers/stereo
 * metadata and such.
 *
 * If clearmode = Don't clear after flip is selected, the backbuffer is restored to previous state
 * after Flip from the AUX buffer copies.
 *
 * This routine is called automatically by PsychFlipWindowBuffers on Screen('Flip') time after
 * the flip has happened.
 *
 * -> Unifies the code in Flip and DrawingFinished.
 *
 */
void PsychPostFlipOperations(PsychWindowRecordType *windowRecord, int clearmode)
{
	GLenum glerr;
    int screenwidth=(int) PsychGetWidthFromRect(windowRecord->rect);
    int screenheight=(int) PsychGetHeightFromRect(windowRecord->rect);
    int stereo_mode=windowRecord->stereomode;

    // Switch to associated GL-Context of windowRecord:
    PsychSetGLContext(windowRecord);

	// Imaging pipeline off?
	if (windowRecord->imagingMode==0 || windowRecord->imagingMode == kPsychNeedFastOffscreenWindows) {
		// Imaging pipeline disabled: This is the old-style way of doing things:
		
		// Set transform matrix to well-defined state:
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		
		// Vertical compression stereo active? This needs special treatment...
		if (stereo_mode==kPsychCompressedTLBRStereo || stereo_mode==kPsychCompressedTRBLStereo) {
			// Yes. We reset the active stereo buffer to 2 == none selected.
			windowRecord->stereodrawbuffer=2;
			// In clearmode==1, aka retain we don't do anything. This way the AUX buffers
			// restore the preflip state automatically. clearmode=2 is undefined by definition ;-)
			if (clearmode==0) {
				// clearmode 0 active. Sterobuffers shall be cleared on flip. We just
				// reset the dirty-flags of the AUX buffers, so backbuffer gets cleared
				// on first use after selection of a new stereo draw buffer:
				windowRecord->auxbuffer_dirty[0]=FALSE;
				windowRecord->auxbuffer_dirty[1]=FALSE;
			}
		}
		// In other stereo modes and mono mode, we don't need to play backbuffer-AUX buffer games,
		// just treat'em as in mono case...
		else if (clearmode!=2) {
			// Reinitialization of back buffer for drawing of next stim requested:
			if (clearmode==1) {
				// We shall not clear the back buffer(s), but restore them to state before "Flip",
				// so previous stim can be incrementally updated where this makes sense.
				// Copy back our backup-copy from AUX buffers:
				glDisable(GL_BLEND);
				
				// Need to do it on both backbuffers when OpenGL native stereo is enabled:
				if (stereo_mode==kPsychOpenGLStereo) {
					glDrawBuffer(GL_BACK_LEFT);
					glReadBuffer(GL_AUX0);
					glRasterPos2i(0, screenheight);
					glCopyPixels(0, 0, screenwidth, screenheight, GL_COLOR);
					glDrawBuffer(GL_BACK_RIGHT);
					glReadBuffer(GL_AUX1);
					glRasterPos2i(0, screenheight);
					glCopyPixels(0, 0, screenwidth, screenheight, GL_COLOR);
				}
				else {
					glDrawBuffer(GL_BACK);
					glReadBuffer(GL_AUX0);
					glRasterPos2i(0, screenheight);
					glCopyPixels(0, 0, screenwidth, screenheight, GL_COLOR);
				}
				
				glEnable(GL_BLEND);
			}
			else {
				// Clearing (both)  back buffer requested:
				if (stereo_mode==kPsychOpenGLStereo) {
					glDrawBuffer(GL_BACK_LEFT);
					PsychGLClear(windowRecord);
					glDrawBuffer(GL_BACK_RIGHT);
					PsychGLClear(windowRecord);
				}
				else {
					glDrawBuffer(GL_BACK);
					PsychGLClear(windowRecord);

				}
			}
		}
		
		// Restore modelview matrix:
		glPopMatrix();
	} // End of traditional postflip implementation for non-imaging mode:
	
	// Imaging pipeline enabled?
    if (windowRecord->imagingMode > 0 && windowRecord->imagingMode != kPsychNeedFastOffscreenWindows) {
		// Yes. This is rather simple. In dontclear=2 mode we do nothing, except reenable
		// the windowRecord as drawing target again. In dontclear=1 mode ditto, because
		// our backing store FBO's already retained a backup of the preflip-framebuffer.
		// Only in dontclear = 0 mode, we need to clear the backing FBO's:
		
		if (clearmode==0) {
			// Select proper viewport and cliprectangles for clearing:
			PsychSetupView(windowRecord);
			
			// Bind left view (or mono view) buffer:
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, windowRecord->fboTable[windowRecord->drawBufferFBO[0]]->fboid);
			// and clear it:
			PsychGLClear(windowRecord);
			
			if (windowRecord->stereomode > 0) {
				// Bind right view buffer for stereo mode:
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, windowRecord->fboTable[windowRecord->drawBufferFBO[1]]->fboid);
				// and clear it:
				PsychGLClear(windowRecord);
			}
		}
		
		// Select proper rendertarget for further drawing ops - restore preflip state:
		PsychSetDrawingTarget(windowRecord);
	}

	glerr = glGetError();
	if (glerr != GL_NO_ERROR) {
		if (glerr == GL_OUT_OF_MEMORY) {
			// Special case: Out of memory after Flip + Postflip operations.
			printf("PTB-Error: The OpenGL graphics hardware encountered an out of memory condition!\n");
			printf("PTB-Error: One cause of this could be that you are running your display at a too\n");
			printf("PTB-Error: high resolution and/or use Anti-Aliasing with a multiSample value that\n");
			printf("PTB-Error: your gfx-card can't handle at the current display resolution. If this is\n");
			printf("PTB-Error: the case, you may have to reduce multiSample level or display resolution.\n");
			printf("PTB-Error: It may help to quit and restart Matlab or Octave before continuing.\n");
		}
		else {
			printf("PTB-Error: The OpenGL graphics hardware encountered the following OpenGL error after flip: %s.\n", gluErrorString(glerr));
		}
	}
	
    PsychTestForGLErrors();

	// Fixup possible low-level framebuffer layout changes caused by commands above this point. Needed from native 10bpc FB support to work reliably.
	PsychFixupNative10BitFramebufferEnableAfterEndOfSceneMarker(windowRecord);

	// EXPERIMENTAL: Execute hook chain for preparation of user space drawing ops: Not thread safe!!!
	if (((windowRecord->flipInfo) && (windowRecord->flipInfo->asyncstate !=0)) && PsychIsHookChainOperational(windowRecord, kPsychUserspaceBufferDrawingPrepare)) {
		// Hooohooo: Someone tries to use this hookchain from within an async flip! We don't support this:
		return;
	}
	else {
		PsychPipelineExecuteHook(windowRecord, kPsychUserspaceBufferDrawingPrepare, NULL, NULL, FALSE, FALSE, NULL, NULL, NULL, NULL);
	}
	
    // Done.
    return;
}

PsychWindowRecordType* PsychGetDrawingTarget(void)
{
	return(currentRendertarget);
}

/* PsychSetDrawingTarget - Set the target window for following drawing ops.
 *
 * Set up 'windowRecord' as the target window for all drawing operations.
 *
 * This routine is usually called from the Screen drawing- and userspace OpenGL <-> Screen
 * state transition routines to setup a specific PTB window as drawing target.
 *
 * It is also called by Screen's internal special image processing routines (e.g,
 * 'TransformTexture' and preparation routines for OpenGL double-buffer swaps to
 * *disable* a window as drawing target, so the low-level internal code is free
 * to do whatever it wants with the system framebuffer or OpenGL FBO's without
 * the danger of interfering/damaging the integrity of onscreen windows and offscreen
 * windows/textures.
 *
 * Basic usage is one of three ways:
 *
 * * PsychSetDrawingTarget(windowRecord) to prepare drawing into the framebuffer of
 * windowRecord, including all behind-the-scenes management, activating the associated
 * OpenGL context for that window and setting up viewport, scissor and projection/modelview
 * matrices etc.
 *
 * * PsychSetDrawingTarget(0x1) to safely reset the drawing target to "None". This will
 * perform all relevant tear-down actions (switching off FBOs, performing backbuffer backups etc.)
 * for the previously active drawing target, then setting the current drawing target to NULL.
 * This command is to be used by PTB internal routines if they need to be able to do
 * whatever they want with the system backbuffer or FBO's via low-level OpenGL calls,
 * without needing to worry about possible side-effects or image corruption in any
 * user defined onscreen/offscreen windows or textures. This is used in routines like
 * 'Flip', 'OpenWindow', 'OpenOffscreenWindow', 'TransformTexture' etc.
 * After this call, the current OpenGL context binding will be undefined! Or to be more
 * accurate: If no window was active then maybe no context will be bound -- Any OpenGL
 * command would cause a crash! If a window was active then that windows context will
 * be bound -- probably not what you want, unless you carefully verified it *is* what
 * you want! ==> Check your assumption wrt. bound context or use PsychSetGLContext()
 * to explicitely set the context you need!
 *
 * * PsychSetDrawingTarget(NULL) is a hard-reset, like the (0x1) case, but without
 * performing sensible tear-down actions. Wrong usage will leave Screen in an undefined
 * state! All current uses of this call have been carefully audited for correctness,
 * usually you don't need this!
 * 
 * The implementation contains two pathways of execution: One for use of imaging pipeline,
 * i.e., with FBO backed framebuffers -- this is the preferred way on modern hardware,
 * as it is more flexible, robust, simpler and faster. For old hardware and non-imaging
 * mode there is a slow path that tries to emulate FBO's with old OpenGL 1.1 mechanisms
 * like glCopyTexImage() et al. This one is relatively limited and inflexible, slow
 * and convoluted!
 *
 * FastPath:
 *
 * If windowRecord corresponds to an onscreen window, the standard framebuffer is
 * selected as drawing target when imagingMode == Use fast offscreen windows, otherwise
 * (full imaging pipe) the FBO of the windows virtual framebuffer is bound.
 * If 'windowRecord' corresponds to a Psychtoolbox texture (or Offscreen Window), we
 * bind the texture as OpenGL framebuffer object, so we have render-to-texture functionality.
 *
 * This requires support for EXT_Framebuffer_object extension, ie., OpenGL 1.5 or higher.
 * On OS/X it requires Tiger 10.4.3 or later.
 *
 * SlowPath:
 *
 * Textures and offscreen windows are implemented via standard OpenGL textures, but as
 * OpenGL FBO's are not available (or disabled), we use the backbuffer as both, backbuffer
 * of an onscreen window, and as a framebuffer for offscreen windows/textures when drawing
 * to them. The routine performs switching between windows (onscreen or offscreen) by
 * saving the backbuffer of the previously active rendertarget into an OpenGL texture via
 * glCopyTexImage() et al., then initializing the backbuffer with the content of the texture
 * of the new drawingtarget by blitting the texture into the framebuffer. Lots of care
 * has to be taken to always backup/restore from/to the proper backbuffer ie. the proper
 * OpenGL context (if multiple are used), to handle the case of transposed or inverted
 * textures (e.g, quicktime engine, videocapture engine, Screen('MakeTexture')), and
 * to handle the case of TEXTURE_2D textures on old hardware that doesn't support rectangle
 * textures! This is all pretty complex and convoluted.
 *
 *
 * This routine only performs state-transitions if necessary, in order to save expensive
 * state switches. It tries to be lazy and avoid work!
 *
 * A special case is calls of this routine from background worker threads not equal
 * to the Matlab/Octave/PTB main execution thread. These threads are part of the async
 * flip implementation on OS/X and Linux. They call code that sometimes calls into this
 * routine. The system is designed to behave properly if this routine just return()'s without
 * doing anything when called from such a workerthread. That's why we check and early-exit
 * in case of non-master thread invocation.
 * 
 */
void PsychSetDrawingTarget(PsychWindowRecordType *windowRecord)
{
    static unsigned int		recursionLevel = 0;
    int						twidth, theight;
    Boolean EmulateOldPTB = PsychPrefStateGet_EmulateOldPTB();

	// Are we called from the main interpreter thread? If not, then we return
	// immediately (No-Op). Worker threads for async flip don't expect this
	// subroutine to execute!
	#if PSYCH_SYSTEM != PSYCH_WINDOWS
		if (!pthread_equal(masterthread, pthread_self())) return;
	#endif

	// Called from main thread --> Work to do.

    // Increase recursion level count:
    recursionLevel++;
    
    // Is this a recursive call?
    if (recursionLevel>1) {
        // Yep. Then we just do nothing:
        recursionLevel--;
        return;
    }
    
	if ((currentRendertarget == NULL) && (windowRecord == 0x1)) {
		// Fast exit: No rendertarget set and savfe reset to "none" requested.
		// Nothing special to do, just revert to NULL case:
		windowRecord = NULL;
	}
	
	// Make sure currentRendertargets context is active if currentRendertarget is non-NULL:
	if (currentRendertarget) {
		PsychSetGLContext(currentRendertarget);
	}
	
	// windowRecord or NULL provided? NULL would mean a warm-start. A value of 0x1 means
	// to backup the current state of bound 'currentRendertarget', then reset to a NULL
	// target, ie. no target. This is like warmstart, but binding any rendertarget later
	// will do the right thing, instead of "forgetting" state info about currentRendertarget.
    if (windowRecord) {
        // State transition requested?
        if (currentRendertarget != windowRecord) {
            // Need to do a switch between drawing target windows:
			
			if (windowRecord == 0x1) {
				// Special case: No new rendertarget, just request to backup the old
				// one and leave it in a tidy, consistent state, then reset to NULL
				// binding. We achiive this by turning windowRecord into a NULL request and
				// unbinding any possibly bound FBO's:
				windowRecord = NULL;

				// Bind system framebuffer if FBO's supported on this system:
				if (glBindFramebufferEXT) glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			}

			// Check if the imaging pipeline is enabled for this window. If so, we will use
			// the fast FBO based rendertarget implementation - unless windowRecord is a NULL target,
			// in which case we're done already:
            if (windowRecord && ((windowRecord->imagingMode & kPsychNeedFastBackingStore) || (windowRecord->imagingMode == kPsychNeedFastOffscreenWindows))) {
                // Imaging pipeline (at least partially) active for this window. Use OpenGL framebuffer objects: This is the fast-path!

				// Switch to new context if needed: This will unbind any pending FBO's in old context, if any:
				PsychSetGLContext(windowRecord);

                // Transition to offscreen rendertarget?
                if (windowRecord->windowType == kPsychTexture) {
                    // Yes. Need to bind the texture as framebuffer object. This only works for rectangle textures.
					if (PsychGetTextureTarget(windowRecord)!=GL_TEXTURE_RECTANGLE_EXT) {
						PsychErrorExitMsg(PsychError_user, "You tried to draw into a special power-of-two offscreen window or texture. Sorry, this is not supported.");
					}
					
					// It also only works on RGB or RGBA textures, not Luminance or LA textures, and the texture needs to be upright.
					// PsychNormalizeTextureOrientation takes care of swapping it upright and converting it into a RGB or RGBA format,
					// if needed. Only if it were an upright non-RGB(A) texture, it would slip through this and trigger an error abort
					// in the following PsychCreateShadowFBO... call. This however can't happen with textures created by 'OpenOffscreenWindow',
					// textures from the Quicktime movie engine, the videocapture engine or other internal sources. Textures created via
					// MakeTexture will be auto-converted as well, unless some special flags to MakeTexture are given.
					// --> The user code needs to do something very unusual and special to trigger an error abort here, and if it triggers
					// one, it will abort with a helpful error message, telling how to fix the problem very simply.
					PsychSetShader(windowRecord, 0);
					PsychNormalizeTextureOrientation(windowRecord);
					
					// Do we already have a framebuffer object for this texture? All textures start off without one,
					// because most textures are just used for drawing them, not drawing *into* them. Therefore we
					// only create a full blown FBO on demand here.
					PsychCreateShadowFBOForTexture(windowRecord, TRUE, -1);

					// Switch to FBO for given texture or offscreen window:
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, windowRecord->fboTable[0]->fboid);

				} // Special setup for offscreen windows or textures finished.
				else {
					// Bind onscreen window as drawing target:
					if (windowRecord->imagingMode == kPsychNeedFastOffscreenWindows) {
						// Only fast offscreen windows active: Onscreen window is the system framebuffer.
						// Revert to it:
						glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
					}
					else {
						// Full pipeline active:
						
						// We either bind the drawBufferFBO for the left channel or right channel, depending
						// on stereo mode and selected stereo buffer:
						if ((windowRecord->stereomode > 0) && (windowRecord->stereodrawbuffer == 1)) {
							// We are in stereo mode and want to draw into the right-eye channel. Bind FBO-1
							glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, windowRecord->fboTable[windowRecord->drawBufferFBO[1]]->fboid);
						}
						else {
							// We are either in stereo mode and want to draw into left-eye channel or we are
							// in mono mode. Bind FBO-0:
							glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, windowRecord->fboTable[windowRecord->drawBufferFBO[0]]->fboid);
						}
					}
				}

				// Fast path for rendertarget switch finished.
            }	// End of fast-path: FBO based processing...
            else {
                // Use standard OpenGL without framebuffer objects for drawing target switch:
                // This code path is executed when the imaging pipeline is disabled. It only uses
				// OpenGL 1.1 functionality so it should work on any piece of gfx-hardware:
				
                // Whatever is bound at the moment needs to be backed-up into a texture...
                // If currentRendertarget is NULL then we've got nothing to back up.
				// If currentRendertarget is using the imaging pipeline in any way, then there's also no
				// need for any backups, as all textures/offscreen windows are backed by FBO's and the
				// system framebuffer is just used as backingstore for onscreen windows, ie., no need
				// to ever backup system framebuffer into any kind of texture based storage.
				// Therefore skip this if any imaging mode is active (i.e., imagingMode is non-zero):
                if (currentRendertarget && (currentRendertarget->imagingMode == 0)) {
                    // There is a bound render target in non-imaging mode: Any backups of its current backbuffer to some
					// texture backing store needed?
                    if (currentRendertarget->windowType == kPsychTexture || (windowRecord && (windowRecord->windowType == kPsychTexture))) {
                        // Ok we transition from- or to a texture. We need to backup the old content:
                        if (EmulateOldPTB) {
                            // OS-9 emulation: frontbuffer = framebuffer, backbuffer = offscreen scratchpad
                            if (PsychIsOnscreenWindow(currentRendertarget)) {
                                // Need to read the content of the frontbuffer to create the backup copy:
                                glReadBuffer(GL_FRONT);
                                glDrawBuffer(GL_FRONT);
                            }
                            else {
                                // Need to read the content of the backbuffer (scratch buffer for offscreen windows) to create the backup copy:
                                glReadBuffer(GL_BACK);
                                glDrawBuffer(GL_BACK);
                            }
                        }
                        
                        // In emulation mode for old PTB, we only need to back up offscreen windows, as they
                        // share the backbuffer as scratchpad. Each onscreen window has its own frontbuffer, so
                        // it will be unaffected by the switch --> No need to backup & restore.
                        if (!EmulateOldPTB || (EmulateOldPTB && !PsychIsOnscreenWindow(currentRendertarget))) {
                            if (currentRendertarget->textureNumber == 0) {
                                // This one is an onscreen window that doesn't have a shadow-texture yet. Create a suitable one.
                                glGenTextures(1, &(currentRendertarget->textureNumber));
                                glBindTexture(PsychGetTextureTarget(currentRendertarget), currentRendertarget->textureNumber);
								// If this system only supports power-of-2 textures, then we'll need a little trick:
								if (PsychGetTextureTarget(currentRendertarget)==GL_TEXTURE_2D) {
									// Ok, we need to create an empty texture of suitable power-of-two size:
									// Now we can do subimage texturing...
									twidth=1; while(twidth < (int) PsychGetWidthFromRect(currentRendertarget->rect)) { twidth = twidth * 2; };
									theight=1; while(theight < (int) PsychGetHeightFromRect(currentRendertarget->rect)) { theight = theight * 2; };
									glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, twidth, theight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
									glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, (int) PsychGetWidthFromRect(currentRendertarget->rect), (int) PsychGetHeightFromRect(currentRendertarget->rect));
								}
								else {
									// Supports rectangle textures. Just create texture as copy of framebuffer:
									glCopyTexImage2D(PsychGetTextureTarget(currentRendertarget), 0, GL_RGBA8, 0, 0, (int) PsychGetWidthFromRect(currentRendertarget->rect), (int) PsychGetHeightFromRect(currentRendertarget->rect), 0); 
								}
                            }
                            else {
								// Texture for this one already exist: Bind and update it:
								twidth  = (int) PsychGetWidthFromRect(currentRendertarget->rect);
								theight = (int) PsychGetHeightFromRect(currentRendertarget->rect);
								
								// If this is a texture in non-normal orientation, we need to swap width and height, and reset orientation
								// to upright:
								if (!PsychIsOnscreenWindow(currentRendertarget)) {
									// Texture. Handle size correctly:
									if ((currentRendertarget->textureOrientation <= 1) && (PsychGetTextureTarget(currentRendertarget)==GL_TEXTURE_2D)) {
										// Transposed power of two texture. Need to realloc texture...
										twidth=1; while(twidth < (int) PsychGetWidthFromRect(currentRendertarget->rect)) { twidth = twidth * 2; };
										theight=1; while(theight < (int) PsychGetHeightFromRect(currentRendertarget->rect)) { theight = theight * 2; };
										glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, twidth, theight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

										// Reassign real size:
										twidth  = (int) PsychGetWidthFromRect(currentRendertarget->rect);
										theight = (int) PsychGetHeightFromRect(currentRendertarget->rect);

										currentRendertarget->surfaceSizeBytes = 4 * twidth * theight; 
									}
									
									// After this backup-op, the texture orientation will be a nice upright one:
									currentRendertarget->textureOrientation = 2;
								}
								
								glBindTexture(PsychGetTextureTarget(currentRendertarget), currentRendertarget->textureNumber);
								if (PsychGetTextureTarget(currentRendertarget)==GL_TEXTURE_2D) {
									// Special case for power-of-two textures:
									glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, twidth, theight);
								}
								else {
									// This would be appropriate but crashes for no good reason on OS-X 10.4.4: glCopyTexSubImage2D(PsychGetTextureTarget(currentRendertarget), 0, 0, 0, 0, 0, (int) PsychGetWidthFromRect(currentRendertarget->rect), (int) PsychGetHeightFromRect(currentRendertarget->rect));                         
									glCopyTexImage2D(PsychGetTextureTarget(currentRendertarget), 0, GL_RGBA8, 0, 0, twidth, theight, 0);
									currentRendertarget->surfaceSizeBytes = 4 * twidth * theight; 
								}
                            }
                        } // Backbuffer -> Texture backup code.
                    } // Transition from- or to a texture.
                } // currentRenderTarget non-NULL.
				
				// At this point we're done with the context and stuff of the old currentRendertarget.
				// Everything backed up.
				
				// A real new rendertarget requested?
				if (windowRecord) {
					// Yes. Activate its OpenGL context:
					PsychSetGLContext(windowRecord);

					// We only blit when a texture was involved, either as previous rendertarget or as new rendertarget:
					if (windowRecord->windowType == kPsychTexture || (currentRendertarget && currentRendertarget->windowType == kPsychTexture)) {
						// OS-9 emulation: frontbuffer = framebuffer, backbuffer = offscreen scratchpad
						if (EmulateOldPTB) {
							// OS-9 emulation: frontbuffer = framebuffer, backbuffer = offscreen scratchpad
							if (PsychIsOnscreenWindow(windowRecord)) {
								// Need to write the content to the frontbuffer to restore from the backup copy:
								glReadBuffer(GL_FRONT);
								glDrawBuffer(GL_FRONT);
							}
							else {
								// Need to write the content to the backbuffer (scratch buffer for offscreen windows) to restore from the backup copy:
								glReadBuffer(GL_BACK);
								glDrawBuffer(GL_BACK);
							}
						}
						
						// In emulation mode for old PTB, we only need to restore offscreen windows, as they
						// share the backbuffer as scratchpad. Each onscreen window has its own frontbuffer, so
						// it will be unaffected by the switch --> No need to backup & restore.
						if (!EmulateOldPTB || (EmulateOldPTB && !PsychIsOnscreenWindow(windowRecord))) {
							// Setup viewport and projections to fit new dimensions of new rendertarget:
							PsychSetupView(windowRecord);
							glPushMatrix();
							glLoadIdentity();

							// Disable any shaders:
							PsychSetShader(windowRecord, 0);
							
							// Now we need to blit the new rendertargets texture into the framebuffer. We need to make
							// sure that alpha-blending is disabled during this blit operation:
							if (glIsEnabled(GL_BLEND)) {
								// Alpha blending enabled. Disable it, blit texture, reenable it:
								glDisable(GL_BLEND);
								PsychBlitTextureToDisplay(windowRecord, windowRecord, windowRecord->rect, windowRecord->rect, 0, 0, 1);
								glEnable(GL_BLEND);
							}
							else {
								// Alpha blending not enabled. Just blit it:
								PsychBlitTextureToDisplay(windowRecord, windowRecord, windowRecord->rect, windowRecord->rect, 0, 0, 1);
							}
							
							glPopMatrix();
							
							// Ok, the framebuffer has been initialized with the content of our texture.
						}
					}	// End of from- to- texture/offscreen window transition...
				}	// End of setup of a real new rendertarget windowRecord...

                // At this point we should have the image of our drawing target in the framebuffer.
                // If this transition didn't involve a switch from- or to a texture aka offscreen window,
                // then the whole switching up to now was a no-op... This way, we optimize for the common
                // case: No drawing to Offscreen windows at all, but proper use of other drawing functions
                // or of MakeTexture.
            } // End of switching code for imaging vs. non-imaging.
            
			// Common code after fast- or slow-path switching:
			
            // Setup viewport, clip rectangle and projections to fit new dimensions of new drawingtarget:
            if (windowRecord) PsychSetupView(windowRecord);
			
            // Update our bookkeeping, set windowRecord as current rendertarget:
            currentRendertarget = windowRecord;
			
			// Transition finished.
        } // End of transition code.
    } // End of if(windowRecord) - then branch...
    else {
        // windowRecord==NULL. Reset of currentRendertarget and framebufferobject requested:

		// Bind system framebuffer if FBO's supported on this system:
        if (glBindFramebufferEXT && currentRendertarget) glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		// Reset current rendertarget to 'none':
        currentRendertarget = NULL;
    }

    // Decrease recursion level tracker:
    recursionLevel--;
    
    return;
}

/* PsychSetupView()  -- Setup proper viewport, clip rectangle and projection
 * matrix for specified window.
 */
void PsychSetupView(PsychWindowRecordType *windowRecord)
{
    // Set viewport to windowsize:
    glViewport(0, 0, (int) PsychGetWidthFromRect(windowRecord->rect), (int) PsychGetHeightFromRect(windowRecord->rect));
    glScissor(0, 0, (int) PsychGetWidthFromRect(windowRecord->rect), (int) PsychGetHeightFromRect(windowRecord->rect));
    
    // Setup projection matrix for a proper orthonormal projection for this framebuffer or window:
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(windowRecord->rect[kPsychLeft], windowRecord->rect[kPsychRight], windowRecord->rect[kPsychBottom], windowRecord->rect[kPsychTop]);

    // Switch back to modelview matrix, but leave it unaltered:
    glMatrixMode(GL_MODELVIEW);
    return;
}

/* Set Screen - global flag which tells PTB if userspace rendering is active or not. */
void PsychSetUserspaceGLFlag(boolean inuserspace)
{
	inGLUserspace = inuserspace;
}

/* Get Screen - global flag which tells if we are in userspace rendering mode: */
boolean PsychIsUserspaceRendering(void)
{
	return(inGLUserspace);
}

int PsychRessourceCheckAndReminder(boolean displayMessage) {
	int i,j = 0;

	#if PSYCH_SYSTEM != PSYCH_LINUX
	// Check for open movies:
	j = PsychGetMovieCount();
	if (j > 0) {
		if (displayMessage && PsychPrefStateGet_Verbosity()>2) {
			printf("\n\nPTB-INFO: There are still %i movies open. Screen('CloseAll') will auto-close them.\n", j);
			printf("PTB-INFO: This may be fine for studies where you only use a single movie, but a large number of open\n");
			printf("PTB-INFO: movies can be an indication that you forgot to dispose no longer needed movie objects\n");
			printf("PTB-INFO: via a proper call to Screen('CloseMovie', moviePtr); , e.g., at the end of each trial. These\n");
			printf("PTB-INFO: stale movies linger around and can consume significant memory and cpu ressources, causing\n");
			printf("PTB-INFO: degraded performance, timing trouble and ultimately out of memory or out of ressource\n");
			printf("PTB-INFO: conditions or even crashes of Matlab/Octave (in rare cases). Please check your code.\n\n");
		}
	}
	#endif

	// Check for open textures and proxies at close time. Might be indicative of missing
	// close calls for releasing texture -- ie. leaked memory:
	i = PsychCountOpenWindows(kPsychTexture) + PsychCountOpenWindows(kPsychProxyWindow);
	
	// Textures open. Give a friendly reminder if either at least 10 textures are remaining or
	// the user asked for verbosity level > 3, ie. very exhaustive info, and at least one texture is remaining.
	if (displayMessage && ((PsychPrefStateGet_Verbosity()>2 && i> 10) || (PsychPrefStateGet_Verbosity() > 3 && i > 0))) {
		printf("\n\nPTB-INFO: There are still %i textures, offscreen windows or proxy windows open. Screen('CloseAll') will auto-close them.\n", i);
		printf("PTB-INFO: This may be fine for studies where you only use a few textures or windows, but a large number of open\n");
		printf("PTB-INFO: textures or offscreen windows can be an indication that you forgot to dispose no longer needed items\n");
		printf("PTB-INFO: via a proper call to Screen('Close', [windowOrTextureIndex]); , e.g., at the end of each trial. These\n");
		printf("PTB-INFO: stale objects linger around and can consume significant memory ressources, causing degraded performance,\n");
		printf("PTB-INFO: timing trouble (if the system has to resort to disk paging) and ultimately out of memory conditions or\n");
		printf("PTB-INFO: crashes. Please check your code. (Screen('Close') is a quick way to release all textures and offscreen windows)\n\n");
	}
	
	// Return total sum of open ressource hogs ;-)
	return(i + j);
}

/* PsychGetCurrentShader() - Returns currently bound GLSL
 * program object, if any. Returns 0 if fixed-function pipeline
 * is active.
 *
 * This needs to distinguish between OpenGL 2.0 and earlier.
 */
int PsychGetCurrentShader(PsychWindowRecordType *windowRecord) {
	int curShader;
	
	if (GLEW_VERSION_2_0) {
		glGetIntegerv(GL_CURRENT_PROGRAM, &curShader);
	}
	else {
		curShader = (int) glGetHandleARB(GL_PROGRAM_OBJECT_ARB);
	}

	return(curShader);
}

/* PsychSetShader() -- Lazily choose a GLSL shader to use for further operations.
 *
 * The routine shall bind the shader 'shader' for the OpenGL context of window
 * 'windowRecord'. It assumes that the OpenGL context for that windowRecord is
 * already bound.
 *
 * This is a wrapper around glUseProgram(). It does nothing if GLSL isn't supported,
 * ie. if gluseProgram() is not available. Otherwise it checks the currently bound
 * shader and only rebinds the new shader if it isn't already bound - avoiding redundant
 * calls to glUseProgram() as such calls might be expensive on some systems.
 *
 * A 'shader' value of zero disables shading and enables fixed-function pipe, as usual.
 * A positive value sets the shader with that handle. Negative values have special
 * meaning in that the select special purpose shaders stored in the 'windowRecord'.
 *
 * Currently the value -1 is defined to choose the windowRecord->defaultDrawShader.
 * That shader can be anything special, zero for fixed function pipe, or e.g., a shader
 * to disable color clamping.
 */
int PsychSetShader(PsychWindowRecordType *windowRecord, int shader)
{
	int oldShader;

	// Have GLSL support?
	if (glUseProgram) {
		// Choose this windowRecords assigned default draw shader if shader == -1:
		if (shader == -1) shader = (int) windowRecord->defaultDrawShader;
		if (shader <  -1) { printf("PTB-BUG: Invalid shader id %i requested in PsychSetShader()! Switching to fixed function.\n", shader); shader = 0; }
		
		// Query currently bound shader:
		oldShader = PsychGetCurrentShader(windowRecord);

		// Switch required? Switch if so:
		if (shader != oldShader) glUseProgram((GLuint) shader);
	}
	else {
		shader = 0;
	}
	
	// Return new bound shader (or zero in case of fixed function only):
	return(shader);
}

/* PsychDetectAndAssignGfxCapabilities()
 *
 * This routine must be called with the OpenGL context of the given 'windowRecord' active,
 * usually once during onscreen window creation.
 *
 * It uses different methods, heuristics, white- and blacklists to determine which capabilities
 * are supported by a given gfx-renderer, or which restrictions apply. It then sets up the
 * gfxcaps bitfield of the windowRecord with proper status bits accordingly.
 *
 * The resulting statusbits can be used by different PTB routines to decide if some feature
 * can be used or if any specific work-arounds need to be enabled for a specific renderer.
 * Most stuff is related to floating point rendering/blending/filtering etc. as recent hw
 * differs in that area.
 */
void PsychDetectAndAssignGfxCapabilities(PsychWindowRecordType *windowRecord)
{
	boolean verbose = (PsychPrefStateGet_Verbosity() > 5) ? TRUE : FALSE;
	
	boolean nvidia = FALSE;
	boolean ati = FALSE;
	GLint maxtexsize=0, maxcolattachments=0, maxaluinst=0;
	
	if (strstr(glGetString(GL_VENDOR), "ATI") || strstr(glGetString(GL_VENDOR), "AMD")) ati = TRUE;
	if (strstr(glGetString(GL_VENDOR), "NVIDIA")) nvidia = TRUE;
	
	while (glGetError());
	glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &maxtexsize);
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxcolattachments);
	if ((glewIsSupported("GL_ARB_fragment_program") || glewIsSupported("GL_ARB_vertex_program")) && glGetProgramivARB!=NULL) glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB, &maxaluinst);
	while (glGetError());
	
	if (verbose) {
		printf("PTB-DEBUG: Interrogating Low-level renderer capabilities for onscreen window with handle %i:\n", windowRecord->windowIndex);
		printf("Indicator variables: FBO's %i, ATI_texture_float %i, ARB_texture_float %i, Vendor %s.\n",
				glewIsSupported("GL_EXT_framebuffer_object"),glewIsSupported("GL_ATI_texture_float"), glewIsSupported("GL_ARB_texture_float"), glGetString(GL_VENDOR));
		printf("Indicator variables: maxcolorattachments = %i, maxrectangletexturesize = %i, maxnativealuinstructions = %i.\n", maxcolattachments, maxtexsize, maxaluinst);
	}
	
	// Is this a GPU with known broken drivers that yield miserable texture creation performance
	// for RGBA8 textures when using the standard optimized settings?
	// As far as we know (June 2008), ATI hardware under MS-Windows and Linux has this driver bugs,
	// at least on X1600 mobile and X1300 desktop:
	if ((PSYCH_SYSTEM == PSYCH_WINDOWS || PSYCH_SYSTEM == PSYCH_LINUX) && ati) {
		// Supposedly: Set the special flag that will trigger alternative parameter selection
		// in PsychCreateTexture():
		windowRecord->gfxcaps |= kPsychGfxCapNeedsUnsignedByteRGBATextureUpload;
	}
	
	// Does usercode want us to override the autmatic choice of optimal texture upload format for RGBA8 textures?
	if (PsychPrefStateGet_ConserveVRAM() & kPsychTextureUploadFormatOverride) {
		// Override! Invert current setting:
		if (windowRecord->gfxcaps & kPsychGfxCapNeedsUnsignedByteRGBATextureUpload) {
			// Clear this caps bit:
			windowRecord->gfxcaps &= (~kPsychGfxCapNeedsUnsignedByteRGBATextureUpload);
		}
		else {
			// Set this caps bit:
			windowRecord->gfxcaps |= kPsychGfxCapNeedsUnsignedByteRGBATextureUpload;
		}
	}
	
	// Support for basic FBO's? Needed for any operation of the imaging pipeline, e.g.,
	// full imaging pipe, fast offscreen windows, Screen('TransformTexture')...
	
	// Check if this system does support OpenGL framebuffer objects and rectangle textures:
	if (glewIsSupported("GL_EXT_framebuffer_object") && (glewIsSupported("GL_EXT_texture_rectangle") || glewIsSupported("GL_ARB_texture_rectangle") || glewIsSupported("GL_NV_texture_rectangle"))) {
		// Basic FBO's utilizing texture rectangle textures as rendertargets are supported.
		// We've got at least RGBA8 rendertargets, including full alpha blending:
		if (verbose) printf("Basic framebuffer objects with rectangle texture rendertargets supported --> RGBA8 rendertargets with blending.\n");
		windowRecord->gfxcaps |= kPsychGfxCapFBO;
		
		// Support for fast inter-framebuffer blits?
		if (glewIsSupported("GL_EXT_framebuffer_blit")) {
			if (verbose) printf("Framebuffer objects support fast blitting between each other.\n");
			windowRecord->gfxcaps |= kPsychGfxCapFBOBlit;			
		}
		
		// Support for multisampled FBO's?
		if (glewIsSupported("GL_EXT_framebuffer_multisample") && (windowRecord->gfxcaps & kPsychGfxCapFBOBlit)) {
			if (verbose) printf("Framebuffer objects support anti-aliasing via multisampling.\n");
			windowRecord->gfxcaps |= kPsychGfxCapFBOMultisample;			
		}
	}

	// ATI_texture_float is supported by R300 ATI cores and later, as well as NV30/40 NVidia cores and later.
	if (glewIsSupported("GL_ATI_texture_float") || glewIsSupported("GL_ARB_texture_float")) {
		// Floating point textures are supported, both 16bpc and 32bpc:
		if (verbose) printf("Hardware supports floating point textures of 16bpc and 32bpc float format.\n");
		windowRecord->gfxcaps |= kPsychGfxCapFPTex16;
		windowRecord->gfxcaps |= kPsychGfxCapFPTex32;
		
		// ATI specific detection logic:
		if (ati && (windowRecord->gfxcaps & kPsychGfxCapFBO)) {
			// ATI hardware with float texture support is a R300 core or later: They support floating point FBO's as well:
			if (verbose) printf("Assuming ATI R300 core or later: Hardware supports basic floating point framebuffers of 16bpc and 32bpc float format.\n");
			windowRecord->gfxcaps |= kPsychGfxCapFPFBO16;
			windowRecord->gfxcaps |= kPsychGfxCapFPFBO32;
			
			// ATI R500 core (X1000 series) can do blending on 16bpc float FBO's, but not R300/R400. They differ
			// in maximum supported texture size (R500 == 4096, R400 == 2560, R300 == 2048) so we use that as detector:
			if (maxtexsize > 4000) {
				// R500 core or later:
				if (verbose) printf("Assuming ATI R500 or later (maxtexsize=%i): Hardware supports floating point blending on 16bpc float format.\n", maxtexsize);
				windowRecord->gfxcaps |= kPsychGfxCapFPBlend16;

				if (verbose) printf("Hardware supports full 32 bit floating point precision shading.\n");
				windowRecord->gfxcaps |= kPsychGfxCapFP32Shading;
				
				// The R600 and later can do full FP blending and texture filtering on 16bpc and 32 bpc float,
				// whereas none of the <= R5xx can do *any* float texture filtering. However, for OS/X, there
				// doesn't seem to be a clear differentiating gl extension or limit to allow distinguishing
				// R600 from earlier cores. The best we can do for now is name matching, which won't work
				// for the FireGL series however, so we also check for maxaluinst > 2000, because presumably,
				// the R600 has a limit of 2048 whereas previous cores only had 512.
				if (strstr(glGetString(GL_RENDERER), "Radeon") && strstr(glGetString(GL_RENDERER), "HD")) {
					// Ok, a Radeon HD 2xxx/3xxx or later -> R600 or later:
					if (verbose) printf("Assuming ATI R600 or later (Matching namestring): Hardware supports floating point blending and filtering on 16bpc and 32bpc float formats.\n");
					windowRecord->gfxcaps |= kPsychGfxCapFPBlend32;
					windowRecord->gfxcaps |= kPsychGfxCapFPFilter16;
					windowRecord->gfxcaps |= kPsychGfxCapFPFilter32;
				}
				else if (maxaluinst > 2000) {
					// Name matching failed, but number ALU instructions is high, so maybe a FireGL with R600 core?
					if (verbose) printf("Assuming ATI R600 or later (Max native ALU inst. = %i): Hardware supports floating point blending and filtering on 16bpc and 32bpc float formats.\n", maxaluinst);
					windowRecord->gfxcaps |= kPsychGfxCapFPBlend32;
					windowRecord->gfxcaps |= kPsychGfxCapFPFilter16;
					windowRecord->gfxcaps |= kPsychGfxCapFPFilter32;					
				}
				
			}
		}
		
		// NVIDIA specific detection logic:
		if (nvidia && (windowRecord->gfxcaps & kPsychGfxCapFBO)) {
			// NVIDIA hardware with float texture support is a NV30 core or later: They support floating point FBO's as well:
			if (verbose) printf("Assuming NV30 core or later...\n");
			
			// Use maximum number of color attachments as differentiator between GeforceFX and GF6xxx/7xxx/....
			if (maxcolattachments > 1) {
				// NV40 core of GF 6000 or later supports at least 16 bpc float texture filtering and framebuffer blending:
				if (verbose) printf("Assuming NV40 core or later (maxcolattachments=%i): Hardware supports floating point blending and filtering on 16bpc float format.\n", maxcolattachments);
                if (verbose) printf("Hardware also supports floating point framebuffers of 16bpc and 32bpc float format.\n");
                windowRecord->gfxcaps |= kPsychGfxCapFPFBO16;
                windowRecord->gfxcaps |= kPsychGfxCapFPFBO32;
				windowRecord->gfxcaps |= kPsychGfxCapFPFilter16;
				windowRecord->gfxcaps |= kPsychGfxCapFPBlend16;	
				
				// NV 40 supports full 32 bit float precision in shaders:
				if (verbose) printf("Hardware supports full 32 bit floating point precision shading.\n");
				windowRecord->gfxcaps |= kPsychGfxCapFP32Shading;
			}
			
			// The Geforce 8xxx/9xxx series and later (G80 cores and later) do support full 32 bpc float filtering and blending:
			// They also support a max texture size of > 4096 texels --> 8192 texels, so we use that as detector:
			if (maxtexsize > 4100) {
				if (verbose) printf("Assuming G80 core or later (maxtexsize=%i): Hardware supports full floating point blending and filtering on 16bpc and 32bpc float format.\n", maxtexsize);
				windowRecord->gfxcaps |= kPsychGfxCapFPBlend32;
				windowRecord->gfxcaps |= kPsychGfxCapFPFilter32;
				windowRecord->gfxcaps |= kPsychGfxCapFPFilter16;
				windowRecord->gfxcaps |= kPsychGfxCapFPBlend16;				
			}
		}		
	}
	
	if (verbose) printf("PTB-DEBUG: Interrogation done.\n\n");
	
	return;
}

// Common (Operating system independent) code to be executed immediately
// before a OS specific double buffer swap request is performed: This
// is called from PsychOSFlipWindowBuffers() within the OS specific variants
// of PsychWindowGlue.c and shall implement special logging actions, workarounds
// etc.
//
// Currently it implements manual syncing of bufferswap requests to VBL onset,
// i.e., waits via beamposition query for VBL onset before returning. This to
// work around setups will totally broken VSYNC support.
void PsychExecuteBufferSwapPrefix(PsychWindowRecordType *windowRecord)
{
    CGDirectDisplayID	cgDisplayID;
    long				vbl_startline, scanline, lastline;

	// Workaround for broken sync-bufferswap-to-VBL support needed?
	if (PsychPrefStateGet_ConserveVRAM() & kPsychBusyWaitForVBLBeforeBufferSwapRequest) {
		// Yes: Sync of bufferswaps to VBL requested?
		if (windowRecord->vSynced) {
			// Sync of bufferswaps to retrace requested:
			// We perform a busy-waiting spin-loop and query current beamposition until
			// beam leaves VBL area:
			
			// Retrieve display handle for beamposition queries:
			PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, windowRecord->screenNumber);
			
			// Retrieve final vbl_startline, aka physical height of the display in pixels:
			PsychGetScreenSize(windowRecord->screenNumber, &scanline, &vbl_startline);

			// Busy-Wait: The special handling of <=0 values makes sure we don't hang here
			// if beamposition queries are broken as well:
			lastline = (long) PsychGetDisplayBeamPosition(cgDisplayID, windowRecord->screenNumber);
			
			if (lastline > 0) {
				// Within video frame. Wait for beamposition wraparound or start of VBL:
				if (PsychPrefStateGet_Verbosity()>9) printf("\nPTB-DEBUG: Lastline beampos = %i\n", (int) lastline);

				scanline = lastline;

				// Wait until entering VBL or wraparound (i.e., VBL skipped). The fudge
				// factor of -1 is to take yet another NVidia bug into account :-(
				while ((scanline < vbl_startline) && (scanline >= lastline - 1)) {
					lastline = (scanline > lastline) ? scanline : lastline;
					if (scanline < (vbl_startline - 100)) PsychYieldIntervalSeconds(0.0);
					scanline = (long) PsychGetDisplayBeamPosition(cgDisplayID, windowRecord->screenNumber);
				}
				if (PsychPrefStateGet_Verbosity()>9) printf("\nPTB-DEBUG: At exit of loop: Lastline beampos = %i, Scanline beampos = %i\n", (int) lastline, (int) scanline);
			}
		}
	}

	return;
}
