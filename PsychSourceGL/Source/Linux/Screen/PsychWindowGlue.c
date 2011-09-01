/*
	PsychToolbox3/Source/Linux/Screen/PsychWindowGlue.c
	
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

#include "Screen.h"

/* These are needed for realtime scheduling control: */
#include <sched.h>
#include <errno.h>

/* XAtom support for setup of transparent windows: */
#include <X11/Xatom.h>

// Number of currently open onscreen windows:
static int x11_windowcount = 0;

// Typedef and fcn-pointer for optional Mesa get swap interval call:
typedef int (*PFNGLXGETSWAPINTERVALMESAPROC)(void);
PFNGLXGETSWAPINTERVALMESAPROC glXGetSwapIntervalMESA = NULL;

#ifndef GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK
#define GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK	0x04000000
#endif

#ifndef GLX_BufferSwapComplete
#define GLX_BufferSwapComplete	1
#endif

typedef struct GLXBufferSwapComplete {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;		/* true if this came from a SendEvent request */
    Display *display;		/* Display the event was read from */
    GLXDrawable drawable;	/* drawable on which event was requested in event mask */
    int event_type;
    int64_t ust;
    int64_t msc;
    int64_t sbc;
} GLXBufferSwapComplete;

/** PsychRealtimePriority: Temporarily boost priority to highest available priority on Linux.
    PsychRealtimePriority(true) enables realtime-scheduling (like Priority(>0) would do in Matlab).
    PsychRealtimePriority(false) restores scheduling to the state before last invocation of PsychRealtimePriority(true),
    it undos whatever the previous switch did.

    We switch to RT scheduling during PsychGetMonitorRefreshInterval() and a few other timing tests in
    PsychOpenWindow() to reduce measurement jitter caused by possible interference of other tasks.
*/
psych_bool PsychRealtimePriority(psych_bool enable_realtime)
{
    static psych_bool old_enable_realtime = FALSE;
    static int   oldPriority = SCHED_OTHER;
    const  int   realtime_class = SCHED_FIFO;
    struct sched_param param, oldparam;

    if (old_enable_realtime == enable_realtime) {
        // No transition with respect to previous state -> Nothing to do.
        return(true);
    }
    
    // Transition requested:    
    if (enable_realtime) {
      // Transition to realtime requested:
      
      // Get current scheduling policy and back it up for later restore:
      oldPriority = sched_getscheduler(0);
      sched_getparam(0, &oldparam);

      // Check if realtime scheduling isn't already active.
      // If we are already in RT mode (e.g., Priority(2) call in Matlab), we skip the switch...
      if (oldPriority != realtime_class) {
	// RT scheduling not yet active -> Switch to it.
	// We use the smallest realtime priority that's available for realtime_class.
	// This way, other processes like watchdogs can preempt us, if needed.
	param.sched_priority = sched_get_priority_min(realtime_class);
	if (sched_setscheduler(0, realtime_class, &param)) {
	  // Failed!
	  if(!PsychPrefStateGet_SuppressAllWarnings()) {
	    printf("PTB-INFO: Failed to enable realtime-scheduling [%s]!\n", strerror(errno));
	    if (errno==EPERM) {
	      printf("PTB-INFO: You need to run Matlab or Octave with root-privileges for this to work.\n");
	    }
	  }
	  errno=0;
	}
      }
    }
    else {
      // Transition from RT to whatever-it-was-before scheduling requested: We just reestablish the backed-up old
      // policy: If the old policy wasn't Non-RT, then we don't switch back...
      if (oldPriority != realtime_class) oldparam.sched_priority = 0;

      if (sched_setscheduler(0, oldPriority, &oldparam)) {
	// Failed!
	if(!PsychPrefStateGet_SuppressAllWarnings()) {
	  printf("PTB-INFO: Failed to disable realtime-scheduling [%s]!\n", strerror(errno));
	  if (errno==EPERM) {
	    printf("PTB-INFO: You need to run Matlab or Octave with root-privileges for this to work.\n");
	  }
	}
	errno=0;
      }
    }

    //printf("PTB-INFO: Realtime scheduling %sabled\n", enable_realtime ? "en" : "dis");

    // Success.
    old_enable_realtime = enable_realtime;
    return(TRUE);
}

/*
    PsychOSOpenOnscreenWindow()
    
    Creates the pixel format and the context objects and then instantiates the context onto the screen.
    
    -The pixel format and the context are stored in the target specific field of the window recored.  Close
    should clean up by destroying both the pixel format and the context.
    
    -We mantain the context because it must be be made the current context by drawing functions to draw into 
    the specified window.
    
    -We maintain the pixel format object because there seems to be now way to retrieve that from the context.
    
    -To tell the caller to clean up PsychOSOpenOnscreenWindow returns FALSE if we fail to open the window. It 
    would be better to just issue an PsychErrorExit() and have that clean up everything allocated outside of
    PsychOpenOnscreenWindow().
*/
psych_bool PsychOSOpenOnscreenWindow(PsychScreenSettingsType *screenSettings, PsychWindowRecordType *windowRecord, int numBuffers, int stereomode, int conserveVRAM)
{
  PsychRectType             screenrect;
  CGDirectDisplayID         dpy;
  int scrnum;
  XSetWindowAttributes attr;
  unsigned long mask;
  Window root;
  Window win;
  GLXContext ctx;
  XVisualInfo *visinfo;
  int i, x, y, width, height;
  GLenum glerr;
  psych_bool fullscreen = FALSE;
  int attrib[38];
  int attribcount=0;
  int depth, bpc;
  int windowLevel;

  // Retrieve windowLevel, an indicator of where non-fullscreen windows should
  // be located wrt. to other windows. 0 = Behind everything else, occluded by
  // everything else. 1 - 999 = At layer windowLevel -> Occludes stuff on layers "below" it.
  // 1000 - 1999 = At highest level, but partially translucent / alpha channel allows to make
  // regions transparent. 2000 or higher: Above everything, fully opaque, occludes everything.
  // 2000 is the default.
  windowLevel = PsychPrefStateGet_WindowShieldingLevel();
  
  // Init userspace GL context to safe default:
  windowRecord->targetSpecific.glusercontextObject = NULL;
  	 
  // Which display depth is requested?
  depth = PsychGetValueFromDepthStruct(0, &(screenSettings->depth));

  // Map the logical screen number to the corresponding X11 display connection handle
  // for the corresponding X-Server connection.
  PsychGetCGDisplayIDFromScreenNumber(&dpy, screenSettings->screenNumber);
  scrnum = PsychGetXScreenIdForScreen(screenSettings->screenNumber);

  // Check if this should be a fullscreen window, and if not, what its dimensions
  // should be:
  PsychGetScreenRect(screenSettings->screenNumber, screenrect);
  if (PsychMatchRect(screenrect, windowRecord->rect)) {
    // This is supposed to be a fullscreen window with the dimensions of
    // the current display/desktop:
    x=0;
    y=0;
    width=PsychGetWidthFromRect(screenrect);
    height=PsychGetHeightFromRect(screenrect);      
    
    // Switch system to fullscreen-mode without changing any settings:
    fullscreen = TRUE;

	// Mark this window as fullscreen window:
	windowRecord->specialflags |= kPsychIsFullscreenWindow;
	
	// Copy absolute screen location and area of window to 'globalrect',
	// so functions like Screen('GlobalRect') can still query the real
	// bounding gox of a window onscreen:
	PsychGetGlobalScreenRect(screenSettings->screenNumber, windowRecord->globalrect);
  }
  else {
    // Window size different from current screen size:
    // A regular desktop window with borders and control icons is requested, e.g., for debugging:
    // Extract settings:
    x=windowRecord->rect[kPsychLeft];
    y=windowRecord->rect[kPsychTop];
    width=PsychGetWidthFromRect(windowRecord->rect);
    height=PsychGetHeightFromRect(windowRecord->rect);
    fullscreen = FALSE;
	
	// Copy absolute screen location and area of window to 'globalrect',
	// so functions like Screen('GlobalRect') can still query the real
	// bounding gox of a window onscreen:
    PsychCopyRect(windowRecord->globalrect, windowRecord->rect);
  }

  // Select requested depth per color component 'bpc' for each channel:
  bpc = 8; // We default to 8 bpc == RGBA8
  if (windowRecord->depth == 30)  { bpc = 10; printf("PTB-INFO: Trying to enable at least 10 bpc fixed point framebuffer.\n"); }
  if (windowRecord->depth == 64)  { bpc = 16; printf("PTB-INFO: Trying to enable 16 bpc fixed point framebuffer.\n"); }
  if (windowRecord->depth == 128) { bpc = 32; printf("PTB-INFO: Trying to enable 32 bpc fixed point framebuffer.\n"); }
  
  // Setup pixelformat descriptor for selection of GLX visual:
  attrib[attribcount++]= GLX_RGBA;       // Use RGBA true-color visual.
  attrib[attribcount++]= GLX_RED_SIZE;   // Setup requested minimum depth of each color channel:
  attrib[attribcount++]= (depth > 16) ? bpc : 1;
  attrib[attribcount++]= GLX_GREEN_SIZE;
  attrib[attribcount++]= (depth > 16) ? bpc : 1;
  attrib[attribcount++]= GLX_BLUE_SIZE;
  attrib[attribcount++]= (depth > 16) ? bpc : 1;
  attrib[attribcount++]= GLX_ALPHA_SIZE;
  // Alpha channel needs special treatment:
  if (bpc != 10) {
	// Non 10 bpc drawable: Request a 'bpc' alpha channel if the underlying framebuffer
	// is in true-color mode ( >= 24 cpp format). If framebuffer is in 16 bpp mode, we
	// don't have/request an alpha channel at all:
	attrib[attribcount++]= (depth > 16) ? bpc : 0; // In 16 bit mode, we don't request an alpha-channel.
  }
  else {
	// 10 bpc drawable: We have a 32 bpp pixel format with R10G10B10 10 bpc per color channel.
	// There are at most 2 bits left for the alpha channel, so we request an alpha channel with
	// minimum size 1 bit --> Will likely translate into a 2 bit alpha channel:
	attrib[attribcount++]= 1;
  }
  
  // Stereo display support: If stereo display output is requested with OpenGL native stereo,
  // we request a stereo-enabled rendering context.
  if(stereomode==kPsychOpenGLStereo) {
    attrib[attribcount++]= GLX_STEREO;
  }

  // Multisampling support:
  if (windowRecord->multiSample > 0) {
    // Request a multisample buffer:
    attrib[attribcount++]= GLX_SAMPLE_BUFFERS_ARB;
    attrib[attribcount++]= 1;
    // Request at least multiSample samples per pixel:
    attrib[attribcount++]= GLX_SAMPLES_ARB;
    attrib[attribcount++]= windowRecord->multiSample;
  }

  // Support for OpenGL 3D rendering requested?
  if (PsychPrefStateGet_3DGfx()) {
    // Yes. Allocate and attach a 24 bit depth buffer and 8 bit stencil buffer:
    attrib[attribcount++]= GLX_DEPTH_SIZE;
    attrib[attribcount++]= 24;
    attrib[attribcount++]= GLX_STENCIL_SIZE;
    attrib[attribcount++]= 8;

	// Alloc an accumulation buffer as well?
	if (PsychPrefStateGet_3DGfx() & 2) {
		// Yes: Alloc accum buffer, request 64 bpp, aka 16 bits integer per color component if possible:
		attrib[attribcount++] = GLX_ACCUM_RED_SIZE;
		attrib[attribcount++] = 16;
		attrib[attribcount++] = GLX_ACCUM_GREEN_SIZE;
		attrib[attribcount++] = 16;
		attrib[attribcount++] = GLX_ACCUM_BLUE_SIZE;
		attrib[attribcount++] = 16;
		attrib[attribcount++] = GLX_ACCUM_ALPHA_SIZE;
		attrib[attribcount++] = 16;
	}
  }

  // Double buffering requested?
  if(numBuffers>=2) {
    // Enable double-buffering:
    attrib[attribcount++]= GLX_DOUBLEBUFFER;

    // AUX buffers for Flip-Operations needed?
    if ((conserveVRAM & kPsychDisableAUXBuffers) == 0) {
      // Allocate one or two (mono vs. stereo) AUX buffers for new "don't clear" mode of Screen('Flip'):
      // Not clearing the framebuffer after "Flip" is implemented by storing a backup-copy of
      // the backbuffer to AUXs before flip and restoring the content from AUXs after flip.
      attrib[attribcount++]= GLX_AUX_BUFFERS;
      attrib[attribcount++]=(stereomode==kPsychOpenGLStereo || stereomode==kPsychCompressedTLBRStereo || stereomode==kPsychCompressedTRBLStereo) ? 2 : 1;
    }
  }

  // It's important that GLX_AUX_BUFFERS is the last entry in the attrib array, see code for glXChooseVisual below...

  // Finalize attric array:
  attrib[attribcount++]= None;

  root = RootWindow( dpy, scrnum );

  // Select matching visual for our pixelformat:
  visinfo = glXChooseVisual( dpy, scrnum, attrib );
  
  if (!visinfo) {
	  // Failed to find matching visual: Could it be related to request for unsupported native 10 bpc framebuffer?
	  if ((windowRecord->depth == 30) && (bpc == 10)) {
		  // 10 bpc framebuffer requested: Let's see if we can get a visual by lowering our demand to 8 bpc:
		  for (i=0; i<attribcount && attrib[i]!=GLX_RED_SIZE; i++);
		  attrib[i+1] = 8;
		  for (i=0; i<attribcount && attrib[i]!=GLX_GREEN_SIZE; i++);
		  attrib[i+1] = 8;
		  for (i=0; i<attribcount && attrib[i]!=GLX_BLUE_SIZE; i++);
		  attrib[i+1] = 8;
		  for (i=0; i<attribcount && attrib[i]!=GLX_ALPHA_SIZE; i++);
		  attrib[i+1] = 1;
		  
		  // Retry:
		  visinfo = glXChooseVisual( dpy, scrnum, attrib );
	  }
  }
  
  if (!visinfo) {
	  // Failed to find matching visual: Could it be related to multisampling?
	  if (windowRecord->multiSample > 0) {
		  // Multisampling requested: Let's see if we can get a visual by
		  // lowering our demand:
		  for (i=0; i<attribcount && attrib[i]!=GLX_SAMPLES_ARB; i++);
		  while(!visinfo && windowRecord->multiSample > 0) {
			  attrib[i+1]--;
			  windowRecord->multiSample--;
			  visinfo = glXChooseVisual( dpy, scrnum, attrib );
		  }
		  
		  // Either we have a valid visual at this point or we still fail despite
		  // requesting zero samples.
		  if (!visinfo) {
			  // We still fail. Disable multisampling by requesting zero multisample buffers:
			  for (i=0; i<attribcount && attrib[i]!=GLX_SAMPLE_BUFFERS_ARB; i++);
			  windowRecord->multiSample = 0;
			  attrib[i+1]=0;
			  visinfo = glXChooseVisual( dpy, scrnum, attrib );
		  }
		}

    // Break out of this if we finally got one...
    if (!visinfo) {
      // Failed to find matching visual: This can happen if we request AUX buffers on a system
      // that doesn't support AUX-buffers. In that case we retry without requesting AUX buffers
      // and output a proper warning instead of failing. For 99% of all applications one can
      // do without AUX buffers anyway...
      //printf("PTB-WARNING: Couldn't enable AUX buffers on onscreen window due to limitations of your gfx-hardware or driver. Some features may be disabled or limited...\n");
      //fflush(NULL);
      
      // Terminate attrib array where the GLX_AUX_BUFFERS entry used to be...
      attrib[attribcount-3] = None;
      
      // Retry...
      visinfo = glXChooseVisual( dpy, scrnum, attrib );
      if (!visinfo && PsychPrefStateGet_3DGfx()) {
	// Ok, retry with a 16 bit depth buffer...
	for (i=0; i<attribcount && attrib[i]!=GLX_DEPTH_SIZE; i++);
	if (attrib[i]==GLX_DEPTH_SIZE && i<attribcount) attrib[i+1]=16;
	printf("PTB-WARNING: Have to use 16 bit depth buffer instead of 24 bit buffer due to limitations of your gfx-hardware or driver. Accuracy of 3D-Gfx may be limited...\n");
	fflush(NULL);
	
	visinfo = glXChooseVisual( dpy, scrnum, attrib );
	if (!visinfo) {
	  // Failed again. Retry with disabled stencil buffer:
	  printf("PTB-WARNING: Have to disable stencil buffer due to limitations of your gfx-hardware or driver. Some 3D Gfx algorithms may fail...\n");
	  fflush(NULL);
	  for (i=0; i<attribcount && attrib[i]!=GLX_STENCIL_SIZE; i++);
	  if (attrib[i]==GLX_STENCIL_SIZE && i<attribcount) attrib[i+1]=0;
	  visinfo = glXChooseVisual( dpy, scrnum, attrib );
	}
      }
    }
  }

  if (!visinfo) {
    printf("\nPTB-ERROR[glXChooseVisual() failed]: Couldn't get any suitable visual from X-Server.\n\n");
    return(FALSE);
  }

  // Set window to non-fullscreen mode if it is a transparent or otherwise special window.
  // This will prevent setting the override_redirect attribute, which would lock out the
  // desktop window compositor:
  if (windowLevel < 2000) fullscreen = FALSE;

  // Also disable fullscreen mode for GUI-like windows:
  if (windowRecord->specialflags & kPsychGUIWindow) fullscreen = FALSE;  

  // Setup window attributes:
  attr.background_pixel = 0;  // Background color defaults to black.
  attr.border_pixel = 0;      // Border color as well.
  attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);  // Dummy colormap assignment.
  attr.event_mask = KeyPressMask | StructureNotifyMask; // | ExposureMask;  // We're only interested in keypress events for GetChar() and StructureNotify to wait for Windows to be mapped.
  attr.override_redirect = (fullscreen) ? 1 : 0;                            // Lock out window manager if fullscreen window requested.
  mask = CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

  // Create our onscreen window:
  win = XCreateWindow( dpy, root, x, y, width, height,
		       0, visinfo->depth, InputOutput,
		       visinfo->visual, mask, &attr );

  // Set hints and properties:
  {
    XSizeHints sizehints;
    sizehints.x = x;
    sizehints.y = y;
    sizehints.width  = width;
    sizehints.height = height;
    sizehints.flags = USSize | USPosition;
    XSetNormalHints(dpy, win, &sizehints);
    XSetStandardProperties(dpy, win, "PTB Onscreen window", "PTB Onscreen window",
			   None, (char **)NULL, 0, &sizehints);
  }

  // Create associated GLX OpenGL rendering context: We use ressource
  // sharing of textures, display lists, FBO's and shaders if 'slaveWindow'
  // is assigned for that purpose as master-window. We request a direct
  // rendering context (True) if possible:
  ctx = glXCreateContext(dpy, visinfo, ((windowRecord->slaveWindow) ? windowRecord->slaveWindow->targetSpecific.contextObject : NULL), True );
  if (!ctx) {
    printf("\nPTB-ERROR:[glXCreateContext() failed] OpenGL context creation failed!\n\n");
    return(FALSE);
  }

  // Store the handles...
  windowRecord->targetSpecific.windowHandle = win;
  windowRecord->targetSpecific.deviceContext = dpy;
  windowRecord->targetSpecific.contextObject = ctx;

  // External 3D graphics support enabled?
  if (PsychPrefStateGet_3DGfx()) {
    // Yes. We need to create an extra OpenGL rendering context for the external
    // OpenGL code to provide optimal state-isolation. The context shares all
    // heavyweight ressources likes textures, FBOs, VBOs, PBOs, display lists and
    // starts off as an identical copy of PTB's context as of here.

    // Create rendering context with identical visual and display as main context, share all heavyweight ressources with it:
    windowRecord->targetSpecific.glusercontextObject = glXCreateContext(dpy, visinfo, windowRecord->targetSpecific.contextObject, True);
    if (windowRecord->targetSpecific.glusercontextObject == NULL) {
      printf("\nPTB-ERROR[UserContextCreation failed]: Creating a private OpenGL context for Matlab OpenGL failed for unknown reasons.\n\n");
      return(FALSE);
    }    
  }
  
  // Release visual info:
  XFree(visinfo);

  // Setup window transparency:
  if ((windowLevel >= 1000) && (windowLevel < 2000)) {
	  // For windowLevels between 1000 and 1999, make the window background transparent, so standard GUI
	  // would be visible, wherever nothing is drawn, i.e., where alpha channel is zero:
	  
	  // Levels 1000 - 1499 and 1500 to 1999 map to a master opacity level of 0.0 - 1.0:	  
	  unsigned int opacity = (unsigned int) (0xffffffff * (((float) (windowLevel % 500)) / 499.0));
	  
	  // Get handle on opacity property of X11:
	  Atom atom_window_opacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
	  
	  // Assign new value for property:
	  XChangeProperty(dpy, win, atom_window_opacity, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &opacity, 1);
  }

  // Show our new window:
  XMapWindow(dpy, win);

  // Spin-Wait for it to be really mapped:
  while (1) {
      XEvent ev;
      XNextEvent(dpy, &ev);
      if (ev.type == MapNotify)
          break;

      PsychYieldIntervalSeconds(0.001);
  }
  
  // Setup window transparency for user input (keyboard and mouse events):
  if (windowLevel < 1500) {
	// Need to try to be transparent for keyboard events and mouse clicks:
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);	
  }

  // Activate the associated rendering context:
  PsychOSSetGLContext(windowRecord);

  // Ok, the OpenGL rendering context is up and running. Auto-detect and bind all
  // available OpenGL extensions via GLEW:
  glerr = glewInit();
  if (GLEW_OK != glerr) {
    /* Problem: glewInit failed, something is seriously wrong. */
    printf("\nPTB-ERROR[GLEW init failed: %s]: Please report this to the forum. Will try to continue, but may crash soon!\n\n", glewGetErrorString(glerr));
    fflush(NULL);
  }
  else {
    printf("PTB-INFO: Using GLEW version %s for automatic detection of OpenGL extensions...\n", glewGetString(GLEW_VERSION));
  }
  
  fflush(NULL);

  // Increase our own open window counter:
  x11_windowcount++;

  // Disable X-Windows screensavers:
  if (x11_windowcount==1) {
    // First window. Disable future use of screensaver:
    XSetScreenSaver(dpy, 0, 0, DefaultBlanking, DefaultExposures);
    // If the screensaver is currently running, forcefully shut it down:
    XForceScreenSaver(dpy, ScreenSaverReset);
  }

  // Some info for the user regarding non-fullscreen and ATI hw:
  if (!(windowRecord->specialflags & kPsychIsFullscreenWindow) && (strstr(glGetString(GL_VENDOR), "ATI"))) {
    printf("PTB-INFO: Some ATI graphics cards may not support proper syncing to vertical retrace when\n");
    printf("PTB-INFO: running in windowed mode (non-fullscreen). If PTB aborts with 'Synchronization failure'\n");
    printf("PTB-INFO: you can disable the sync test via call to Screen('Preference', 'SkipSyncTests', 1); .\n");
    printf("PTB-INFO: You won't get proper stimulus onset timestamps though, so windowed mode may be of limited use.\n");
  }
  fflush(NULL);

  // Check for availability of VSYNC extension:
  
  // First we try if the MESA variant of the swap control extensions is available. It has two advantages:
  // First, it also provides a function to query the current swap interval. Second it allows to set a
  // zero swap interval to dynamically disable sync to retrace, just as on OS/X and Windows:
  if (strstr(glXQueryExtensionsString(dpy, scrnum), "GLX_MESA_swap_control")) {
	// Bingo! Bind Mesa variant of setup call to sgi setup call, just to simplify the code
	// that actually uses the setup call -- no special cases or extra code needed there :-)
	// This special glXSwapIntervalSGI() call will simply accept an input value of zero for
	// disabling vsync'ed bufferswaps as a valid input parameter:
	glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC) glXGetProcAddressARB("glXSwapIntervalMESA");
	
	// Additionally bind the Mesa query call:
	glXGetSwapIntervalMESA = (PFNGLXGETSWAPINTERVALMESAPROC) glXGetProcAddressARB("glXGetSwapIntervalMESA");
	printf("PTB-INFO: Using GLX_MESA_swap_control extension for control of vsync.\n");
  }
  else {
	// Unsupported. Disable the get call:
	glXGetSwapIntervalMESA = NULL;
  }

  // Special case: Buggy ATI driver: Supports the VSync extension and glXSwapIntervalSGI, but provides the
  // wrong extension namestring "WGL_EXT_swap_control" (from MS-Windows!), so GLEW doesn't auto-detect and
  // bind the extension. If this special case is present, we do it here manually ourselves:
  if ( (glXSwapIntervalSGI == NULL) && (strstr(glGetString(GL_EXTENSIONS), "WGL_EXT_swap_control") != NULL) ) {
	// Looks so: Bind manually...
	glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC) glXGetProcAddressARB("glXSwapIntervalSGI");
  }

  // Extension finally supported?
  if (glXSwapIntervalSGI==NULL || ( strstr(glXQueryExtensionsString(dpy, scrnum), "GLX_SGI_swap_control")==NULL &&
	  strstr(glGetString(GL_EXTENSIONS), "WGL_EXT_swap_control")==NULL && strstr(glXQueryExtensionsString(dpy, scrnum), "GLX_MESA_swap_control")==NULL )) {
	  // No, total failure to bind extension:
	  glXSwapIntervalSGI = NULL;
	  printf("PTB-WARNING: Your graphics driver doesn't allow me to control syncing wrt. vertical retrace!\n");
	  printf("PTB-WARNING: Please update your display graphics driver as soon as possible to fix this.\n");
	  printf("PTB-WARNING: Until then, you can manually enable syncing to VBL somehow in a manner that is\n");
	  printf("PTB-WARNING: dependent on the type of gfx-card and driver. Google is your friend...\n");
  }
  fflush(NULL);

  // First opened onscreen window? If so, we try to map GPU MMIO registers
  // to enable beamposition based timestamping and other special goodies:
  if (x11_windowcount == 1) PsychScreenMapRadeonCntlMemory();

  // Ok, we should be ready for OS independent setup...
  fflush(NULL);

  // Check if GLX_INTEL_swap_event extension is supported. Enable swap completion event
  // delivery for our window, if so:
  // TODO FIXME This requires GLX 1.3, and therefore use of new glXCreateWindow() API's etc. to
  // generate compatible GLXDrawable's. --> Needs major rework in our setup code to make it happen.
  // Disable for now.
/*
  if (glXSelectEvent && strstr(glXQueryExtensionsString(dpy, scrnum), "GLX_INTEL_swap_event") && getenv("INTEL_swap_event")) {
	// Wait for X-Server to settle...
        XSync(dpy, 1);
	glXSelectEvent(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, (unsigned long) GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
	printf("PTB-INFO: Use of GLX_INTEL_swap_event extension enabled.\n");
  }
*/
  // Wait for X-Server to settle...
  XSync(dpy, 1);

  // Wait 250 msecs extra to give desktop compositor a chance to settle:
  PsychYieldIntervalSeconds(0.25);

  // Well Done!
  return(TRUE);
}


/*
    PsychOSOpenOffscreenWindow()
    
    Accept specifications for the offscreen window in the platform-neutral structures, convert to native CoreGraphics structures,
    create the surface, allocate a window record and record the window specifications and memory location there.
	
	TO DO:  We need to walk down the screen number and fill in the correct value for the benefit of TexturizeOffscreenWindow
*/
psych_bool PsychOSOpenOffscreenWindow(double *rect, int depth, PsychWindowRecordType **windowRecord)
{
  // This function is obsolete and does nothing.
  return(FALSE);
}

/*
    PsychOSGetPostSwapSBC() -- Internal method for now, used in close window path.
 */
static psych_int64 PsychOSGetPostSwapSBC(PsychWindowRecordType *windowRecord)
{
	psych_int64 ust, msc, sbc;
	sbc = 0;

	#ifdef GLX_OML_sync_control
	// Extension unsupported or known to be defective? Return "damage neutral" 0 in that case:
	if ((NULL == glXWaitForSbcOML) || (windowRecord->specialflags & kPsychOpenMLDefective)) return(0);

	// Extension supported: Perform query and error check.
	if (!glXWaitForSbcOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, 0, &ust, &msc, &sbc)) {
		// Failed! Return a "damage neutral" result:
		return(0);
	}
	#endif
	return(sbc);
}

void PsychOSCloseWindow(PsychWindowRecordType *windowRecord)
{
  Display* dpy = windowRecord->targetSpecific.deviceContext;

  // Check if we are trying to close the window after it had an "odd" (== non-even)
  // number of bufferswaps. If so, we execute one last bufferswap to make the count
  // even. This means that if this window was swapped via page-flipping, the system
  // should end with the same backbuffer-frontbuffer assignment as the one prior
  // to opening the window. This may help sidestep certain bugs in compositing desktop
  // managers (e.g., Compiz).
  if (PsychOSGetPostSwapSBC(windowRecord) % 2) {
	// Uneven count. Submit a swapbuffers request and wait for it to truly finish:

	// We have to rebind the OpenGL context for this swapbuffers call to work around some
	// mesa bug for intel drivers which would cause a crash without context:
	glXMakeCurrent(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, windowRecord->targetSpecific.contextObject);

	PsychOSFlipWindowBuffers(windowRecord);
	PsychOSGetPostSwapSBC(windowRecord);
  }

  if (PsychPrefStateGet_Verbosity() > 5) {
	printf("PTB-DEBUG:PsychOSCloseWindow: Closing with a final swapbuffers count of %i.\n", (int) PsychOSGetPostSwapSBC(windowRecord));
  }

  // Detach OpenGL rendering context again - just to be safe!
  glXMakeCurrent(windowRecord->targetSpecific.deviceContext, None, NULL);

  // Delete rendering context:
  glXDestroyContext(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.contextObject);
  windowRecord->targetSpecific.contextObject=NULL;

  // Delete userspace context, if any:
  if (windowRecord->targetSpecific.glusercontextObject) {
    glXDestroyContext(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.glusercontextObject);
    windowRecord->targetSpecific.glusercontextObject = NULL;
  }

  // Wait for X-Server to settle...
  XSync(dpy, 0);

  // Close & Destroy the window:
  XUnmapWindow(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle);

  // Wait for X-Server to settle...
  XSync(dpy, 0);

  XDestroyWindow(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle);
  windowRecord->targetSpecific.windowHandle=0;

  // Wait for X-Server to settle...
  XSync(dpy, 0);

  // Release device context: We just release the reference. The connection to the display is
  // closed somewhere else.
  windowRecord->targetSpecific.deviceContext=NULL;

  // Decrement global count of open onscreen windows:
  x11_windowcount--;

  // Was this the last window?
  if (x11_windowcount<=0) {
    x11_windowcount=0;

    // (Re-)enable X-Windows screensavers if they were enabled before opening windows:
    // Set screensaver to previous settings, potentially enabling it:
    XSetScreenSaver(dpy, -1, 0, DefaultBlanking, DefaultExposures);
    
    // Unmap/release possibly mapped device memory: Defined in PsychScreenGlue.c
    PsychScreenUnmapDeviceMemory();
  }

  // Done.
  return;
}

/*
    PsychOSGetVBLTimeAndCount()

    Returns absolute system time of last VBL and current total count of VBL interrupts since
    startup of gfx-system for the given screen. Returns a time of -1 and a count of 0 if this
    feature is unavailable on the given OS/Hardware configuration.
*/
double  PsychOSGetVBLTimeAndCount(PsychWindowRecordType *windowRecord, psych_uint64* vblCount)
{
	unsigned int	vsync_counter = 0;
	psych_uint64	ust, msc, sbc;
	
	#ifdef GLX_OML_sync_control
	// Ok, this will return VBL count and last VBL time via the OML GetSyncValuesOML call
	// if that extension is supported on this setup. As of mid 2009 i'm not aware of any
	// affordable graphics card that would support this extension, but who knows??
	if ((NULL != glXGetSyncValuesOML) && !(windowRecord->specialflags & kPsychOpenMLDefective) && (glXGetSyncValuesOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, (int64_t*) &ust, (int64_t*) &msc, (int64_t*) &sbc))) {
		*vblCount = msc;
		if ((PsychGetKernelTimebaseFrequencyHz() > 10000) && !(windowRecord->specialflags & kPsychNeedOpenMLWorkaround1)) {
			// Convert ust into regular GetSecs timestamp:
			// At least we hope this conversion is correct...
			return( PsychOSMonotonicToRefTime(((double) ust) / PsychGetKernelTimebaseFrequencyHz()) );
		}
		else {
			// Last VBL timestamp unavailable:
			return(-1);
		}
	}
	#else
	#warning GLX_OML_sync_control unsupported! Compile with -std=gnu99 to enable it!
	#endif

	// Do we have SGI video sync extensions?
	if (NULL != glXGetVideoSyncSGI) {
		// Retrieve absolute count of vbl's since startup:
		glXGetVideoSyncSGI(&vsync_counter);
		*vblCount = (psych_uint64) vsync_counter;
		
		// Retrieve absolute system time of last retrace, convert into PTB standard time system and return it:
		// Not yet supported on Linux:
		return(-1);
	}
	else {
		// Unsupported :(
		*vblCount = 0;
		return(-1);
	}
}

/* PsychOSGetSwapCompletionTimestamp()
 *
 * Retrieve a very precise timestamp of doublebuffer swap completion by means
 * of OS specific facilities. This function is optional. If the underlying
 * OS/drier/GPU combo doesn't support a high-precision, high-reliability method
 * to query such timestamps, the function should return -1 as a signal that it
 * is unsupported or (temporarily) unavailable. Higher level timestamping code
 * should use/prefer timestamps returned by this function over other timestamps
 * provided by other mechanisms if possible. Calling code must be prepared to
 * use alternate timestamping methods if this method fails or returns a -1
 * unsupported error. Calling code must expect this function to block until
 * swap completion.
 *
 * Input argument targetSBC: Swapbuffers count for which to wait for. A value
 * of zero means to block until all pending bufferswaps for windowRecord have
 * completed, then return the timestamp of the most recently completed swap.
 *
 * A value of zero is recommended.
 *
 * Returns: Precise and reliable swap completion timestamp in seconds of
 * system time in variable referenced by 'tSwap', and msc value of completed swap,
 * or a negative value on error (-1 == unsupported, -2/-3 == Query failed).
 *
 */
psych_int64 PsychOSGetSwapCompletionTimestamp(PsychWindowRecordType *windowRecord, psych_int64 targetSBC, double* tSwap)
{
	psych_int64 ust, msc, sbc;
	msc = -1;

	#ifdef GLX_OML_sync_control
	
	// Extension unsupported or known to be defective? Return -1 "unsupported" in that case:
	if ((NULL == glXWaitForSbcOML) || (windowRecord->specialflags & kPsychOpenMLDefective)) return(-1);

	if (PsychPrefStateGet_Verbosity() > 11) printf("PTB-DEBUG:PsychOSGetSwapCompletionTimestamp: Supported. Calling with targetSBC = %lld.\n", targetSBC);

	// Extension supported: Perform query and error check.
	if (!glXWaitForSbcOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, targetSBC, &ust, &msc, &sbc)) {
		// OpenML supposed to be supported and in good working order according to startup check?
		if (windowRecord->gfxcaps & kPsychGfxCapSupportsOpenML) {
			// Yes. Then this is a new failure condition and we report it as such:
			if (PsychPrefStateGet_Verbosity() > 11) {
				printf("PTB-DEBUG:PsychOSGetSwapCompletionTimestamp: glXWaitForSbcOML() failed! Failing with rc = -2.\n");
			}
			return(-2);
		}
		
		// No. Failing this call is kind'a expected, so we don't make a big fuss on each
		// failure but return "unsupported" rc, so calling code can try fallback-path without
		// making much noise:
		return(-1);
	}
	
	// Check for valid return values: A zero ust or msc means failure, except for results from nouveau,
	// because there it is "expected" to get a constant zero return value for msc, at least when running
	// on top of a pre Linux-3.2 kernel:
	if ((ust == 0) || ((msc == 0) && !strstr((char*) glGetString(GL_VENDOR), "nouveau"))) {
		// Ohoh:
		if (PsychPrefStateGet_Verbosity() > 11) {
			printf("PTB-DEBUG:PsychOSGetSwapCompletionTimestamp: Invalid return values ust = %lld, msc = %lld from call with success return code (sbc = %lld)! Failing with rc = -1.\n", ust, msc, sbc);
		}
		
		// Return with "unsupported" rc, so calling code can try fallback-path:
		return(-1);
	}

	// Success. Translate ust into system time in seconds:
	if (tSwap) *tSwap = PsychOSMonotonicToRefTime(((double) ust) / PsychGetKernelTimebaseFrequencyHz());

	// If we are running on a slightly incomplete nouveau-kms driver which always returns a zero msc,
	// we need to get good ust,msc,sbc values for later use as reference and as return value via an
	// extra roundtrip to the kernel. The most important info, the swap completion timestamp, aka ust
	// as returned from glXWaitForSbcOML() has already been converted into GetSecs() timebase and returned
	// in tSwap, so it is ok to overwrite ust here:
	if (msc == 0) {
		if (!glXGetSyncValuesOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, &ust, &msc, &sbc)) {
			// Ohoh:
			if (PsychPrefStateGet_Verbosity() > 11) {
				printf("PTB-DEBUG:PsychOSGetSwapCompletionTimestamp: Invalid return values ust = %lld, msc = %lld from glXGetSyncValuesOML() call with success return code (sbc = %lld)! Failing with rc = -1.\n", ust, msc, sbc);
			}
		
			// Return with "unsupported" rc, so calling code can try fallback-path:
			return(-1);
		}
	}

	// Update cached reference values for future swaps:
	windowRecord->reference_ust = ust;
	windowRecord->reference_msc = msc;
	windowRecord->reference_sbc = sbc;

	if (PsychPrefStateGet_Verbosity() > 11) printf("PTB-DEBUG:PsychOSGetSwapCompletionTimestamp: Success! refust = %lld, refmsc = %lld, refsbc = %lld.\n", ust, msc, sbc);

	// Experimental support for INTEL_swap_event extension enabled? Process swap events if so:
	// TODO FIXME Disabled for now. Needs GLX 1.3 API's and special setup code...
/*	if ((PsychPrefStateGet_Verbosity() > 11) && glXGetSelectedEvent) {
		unsigned long glxmask;
		glXGetSelectedEvent(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, &glxmask);
		if (glxmask & GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK) {
			// INTEL_swap_event delivery enabled and requested. Try to fetch one:
			int error_base, event_base;
			glXQueryExtension(windowRecord->targetSpecific.deviceContext, &error_base, &event_base);
			
			while(TRUE) {
				XEvent evt;
				printf("PTB-DEBUG: Fetching X-Event...\n"); fflush(NULL);
				XNextEvent(windowRecord->targetSpecific.deviceContext, &evt);
				
				// We're only interested in GLX_BufferSwapComplete events:
				if (evt.type == event_base + GLX_BufferSwapComplete) {
					// Cast to proper event type:
					GLXBufferSwapComplete *sce = (GLXBufferSwapComplete*) &evt;
					printf("SWAPEVENT: OurWin=%i ust = %lld, msc = %lld, sbc = %lld, type %s.\n", (int) (sce->drawable == windowRecord->targetSpecific.windowHandle), sce->ust, sce->msc, sce->sbc, (sce->event_type == GLX_FLIP_COMPLETE_INTEL) ? "PAGEFLIP" : "BLIT/EXCHANGE");
					break;
				}
			}
		}
	}
*/
	#endif
	
	// Return msc of swap completion:
	return(msc);
}

/* PsychOSInitializeOpenML() - Linux specific function.
 *
 * Performs basic initialization of the OpenML OML_sync_control extension.
 * Performs basic and extended correctness tests and disables extension if it
 * is unreliable, or enables workarounds for partially broken extensions.
 *
 * Called from PsychDetectAndAssignGfxCapabilities() as part of the PsychOpenOffscreenWindow()
 * procedure for a window with OpenML support.
 *
 */
void PsychOSInitializeOpenML(PsychWindowRecordType *windowRecord)
{
	#ifdef GLX_OML_sync_control

	psych_int64 ust, msc, sbc, oldmsc, oldust, finalmsc;
	psych_bool failed = FALSE;
	
	// Enable rendering context of window:
	PsychSetGLContext(windowRecord);

	// Perform a wait for 3 video refresh cycles to get valid (ust,msc,sbc)
	// values for initialization of windowRecord's cached values:
	if (!glXGetSyncValuesOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, &ust, &msc, &sbc) || (msc == 0) ||
		!glXWaitForMscOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, msc + 3, 0, 0, &ust, &msc, &sbc) || (ust == 0)) {
		
		// Basic OpenML functions failed?!? Not good! Disable OpenML, warn user:
		windowRecord->gfxcaps &= ~kPsychGfxCapSupportsOpenML;
		
		if (PsychPrefStateGet_Verbosity() > 1) {
			printf("PTB-WARNING: At least one test call for OpenML OML_sync_control extension failed! Will disable OpenML and revert to fallback implementation.\n");
		}

		return;
	}

	// Have a valid (ust, msc) baseline. Store it in windowRecord for future use:
	windowRecord->reference_ust = ust;
	windowRecord->reference_msc = msc;
	windowRecord->reference_sbc = sbc;

	// Perform correctness test for glXGetSyncValuesOML() over a time span
	// of 6 video refresh cycles. This checks for a limitation that is present
	// in all shipping Linux kernels up to at least version 2.6.36, possibly
	// also in 2.6.37 depending on MK's progress with this feature:
	finalmsc = msc + 6;
	oldmsc = msc;
	oldust = ust;
	
	while ((msc < finalmsc) && !failed) {
		// Wait a quarter millisecond:
		PsychWaitIntervalSeconds(0.000250);
		
		// Query current (msc, ust):
		if (!glXGetSyncValuesOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, &ust, &msc, &sbc)) {
			// Query failed!
			failed = TRUE;
		}
		
		// Has msc changed since last query due to a regular msc increment, i.e., a new video refresh interval?
		if (msc != oldmsc) {
			// Yes. Update reference values for test:
			oldmsc = msc;
			oldust = ust;
		}
		
		// ust must be equal to oldust at this point, either because a msc increment has updated
		// the ust for the new vblank interval in lock-step and our code above has updated oldust
		// accordingly, or because no msc increment has happened, in which case ust should stay
		// unchanged as well, ie., ust == oldust. If ust and oldust are different then that means
		// that ust has changed its value in the middle of a refresh interval without an intervening
		// vblank. This would happen if glXGetSyncValuesOML() is defective and doesn't return ust
		// timestamps locked to vblank / msc increments, but simply system time values.
		if (ust != oldust) {
			// Failure of glXGetSyncValuesOML()! This is a broken implementation which needs
			// our workaround:
			failed = TRUE;			
		}

		// Repeat test loop:
	}
	
	// Failed or succeeded?
	if (failed) {
		// Failed! Enable workaround and optionally inform user:
		windowRecord->specialflags |= kPsychNeedOpenMLWorkaround1;
		
		if (PsychPrefStateGet_Verbosity() > 1) {
			printf("PTB-INFO: OpenML OML_sync_control implementation with problematic glXGetSyncValuesOML() function detected. Enabling workaround for ok performance.\n");
		}
	}
	#else
		// Disable extension:
		windowRecord->gfxcaps &= ~kPsychGfxCapSupportsOpenML;	
	#endif

	return;
}

/*
    PsychOSScheduleFlipWindowBuffers()
    
    Schedules a double buffer swap operation for given window at a given
	specific target time or target refresh count in a specified way.
	
	This uses OS specific API's and algorithms to schedule the asynchronous
	swap. This function is optional, target platforms are free to not implement
	it but simply return a "not supported" status code.
	
	Arguments:
	
	windowRecord - The window to be swapped.
	tWhen        - Requested target system time for swap. Swap shall happen at first
				   VSync >= tWhen.
	targetMSC	 - If non-zero, specifies target msc count for swap. Overrides tWhen.
	divisor, remainder - If set to non-zero, msc at swap must satisfy (msc % divisor) == remainder.
	specialFlags - Additional options, a bit field consisting of single bits that can be or'ed together:
				   1 = Constrain swaps to even msc values, 2 = Constrain swaps to odd msc values. (Used for frame-seq. stereo field selection)
	
	Return value:
	 
	Value greater than or equal to zero on success: The target msc for which swap is scheduled.
	Negative value: Error. Function failed. -1 == Function unsupported on current system configuration.
	-2 ... -x == Error condition.
	
*/
psych_int64 PsychOSScheduleFlipWindowBuffers(PsychWindowRecordType *windowRecord, double tWhen, psych_int64 targetMSC, psych_int64 divisor, psych_int64 remainder, unsigned int specialFlags)
{
	psych_int64 ust, msc, sbc, rc;
	double tNow, tMsc;
	
	// Linux: If this is implemented then it is implemented via the OpenML OML_sync_control extension.
	// Is the extension supported by the system and enabled by Psychtoolbox? If not, we return
	// a "not-supported" status code of -1 and turn into a no-op:
	if (!(windowRecord->gfxcaps & kPsychGfxCapSupportsOpenML)) return(-1);

	// Extension supported and enabled. Use it.
	#ifdef GLX_OML_sync_control

	// Enable rendering context of window:
	PsychSetGLContext(windowRecord);
	
	// According to OpenML spec, a glFlush() isn't implicitely performed by
	// glXSwapBuffersMscOML(). Therefore need to do it ourselves, although
	// some implementations may do it anyway:
	if (!windowRecord->PipelineFlushDone) glFlush();
	windowRecord->PipelineFlushDone = TRUE;

	// Non-Zero targetMSC provided to directy specify the msc on which swap should happen?
	// If so, then we can skip computation and directly call with that targetMSC:
	if (targetMSC == 0) {
		// No: targetMSC shall be computed from given tWhen system target time.
		// Get current (msc,ust) reference values for computation.
		
		// Get current values for (msc, ust, sbc) the textbook way: Return error code -2 on failure:
		if (!glXGetSyncValuesOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, &ust, &msc, &sbc)) return(-2);
		
		// glXGetSyncValuesOML() known to return totally bogus ust timestamps? Or ust <= 0 returned,
		// which means a temporary (EAGAIN style) failure?
		if ((windowRecord->specialflags & kPsychNeedOpenMLWorkaround1) || (ust <= 0)) {
			// Useless ust returned. We need to recover a useable reference (msc, ust) pair via
			// trickery instead. Check if tWhen is at least 4 video refresh cycles in the future.
			if ((ust <= 0) && (PsychPrefStateGet_Verbosity() > 11)) printf("PTB-DEBUG:PsychOSScheduleFlipWindowBuffers: Invalid ust %lld returned by query. Current msc = %lld.\n", ust, msc);

			PsychGetAdjustedPrecisionTimerSeconds(&tNow);
			if (((tWhen - tNow) / windowRecord->VideoRefreshInterval) > 4.0) {
				// Yes. We have some time until deadline. Use a blocking wait for at
				// least 2 video refresh cycles. glXWaitForMscOML() doesn't have known
				// issues iff it has to wait for a msc that is in the future, ie., it has
				// to perform a blocking wait. In that case it will return a valid (msc, ust)
				// pair on return from blocking wait. Wait until msc+2 is reached and retrieve
				// updated (msc, ust):
				if (PsychPrefStateGet_Verbosity() > 11) printf("PTB-DEBUG:PsychOSScheduleFlipWindowBuffers: glXWaitForMscOML until msc = %lld, now msc = %lld.\n", msc + 2, msc);
				if (!glXWaitForMscOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, msc + 2, 0, 0, &ust, &msc, &sbc)) return(-3);
			}
			else {
				// No. Swap deadline is too close to current time. We have no option other than
				// simply using the last cached values in our windowRecord and hoping that they
				// will be good enough:
				ust = windowRecord->reference_ust;
				msc = windowRecord->reference_msc;
				sbc = windowRecord->reference_sbc;
			}
		}
		
		// Have a valid (ust, msc) baseline. Store it in windowRecord for future use:
		windowRecord->reference_ust = ust;
		windowRecord->reference_msc = msc;
		windowRecord->reference_sbc = sbc;
		
		// Compute targetMSC for given baseline and target time tWhen:
		tMsc = PsychOSMonotonicToRefTime(((double) ust) / PsychGetKernelTimebaseFrequencyHz());
		targetMSC = msc + ((psych_int64)(floor((tWhen - tMsc) / windowRecord->VideoRefreshInterval) + 1));
	}
	
	// Clamp targetMSC to a positive non-zero value:
	if (targetMSC <= 0) targetMSC = 1;

	// Swap at specific even or odd frame requested? This is useful for frame-sequential stereo
	// presentation that shall start its presentation at a specific eye-view:
	if (specialFlags & (0x1 | 0x2)) {
		// Yes. Setup (divisor,remainder) constraint so that
		// 0x1 maps to even target frames, and 0x2 maps to odd
		// target frames:
		divisor = 2;
		remainder = (specialFlags & 0x1) ? 0 : 1;
		// Make sure initial targetMSC obeys (divisor,remainder) constraint:
		targetMSC += (targetMSC % divisor == remainder) ? 0 : 1; 
	}

	if (PsychPrefStateGet_Verbosity() > 12) printf("PTB-DEBUG:PsychOSScheduleFlipWindowBuffers: Submitting swap request for targetMSC = %lld, divisor = %lld, remainder = %lld.\n", targetMSC, divisor, remainder);

	// Ok, we have a valid final targetMSC. Schedule a bufferswap for that targetMSC, taking a potential
	// (divisor, remainder) constraint into account:
	rc = glXSwapBuffersMscOML(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, targetMSC, divisor, remainder);

	// Failed? Return -4 error code if so:
	if (rc == -1) return(-4);

	// Keep track of target_sbc and targetMSC, who knows for what they might be good for?
	windowRecord->target_sbc = rc;
	windowRecord->lastSwaptarget_msc = targetMSC;

	#else
		// No op branch in case OML_sync_control isn't enabled at compile time:
		return(-1);
	#endif

	// Successfully scheduled the swap request: Return targetMSC for which it was scheduled:
	return(targetMSC);
}

/*
    PsychOSFlipWindowBuffers()
    
    Performs OS specific double buffer swap call.
*/
void PsychOSFlipWindowBuffers(PsychWindowRecordType *windowRecord)
{
	// Execute OS neutral bufferswap code first:
	PsychExecuteBufferSwapPrefix(windowRecord);
	
	// Trigger the "Front <-> Back buffer swap (flip) (on next vertical retrace)":
	glXSwapBuffers(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle);
}

/* Enable/disable syncing of buffer-swaps to vertical retrace. */
void PsychOSSetVBLSyncLevel(PsychWindowRecordType *windowRecord, int swapInterval)
{
  int error, myinterval;

  // Enable rendering context of window:
  PsychSetGLContext(windowRecord);

  // Store new setting also in internal helper variable, e.g., to allow workarounds to work:
  windowRecord->vSynced = (swapInterval > 0) ? TRUE : FALSE;

  // Try to set requested swapInterval if swap-control extension is supported on
  // this Linux machine. Otherwise this will be a no-op...
  // Note: On Mesa, glXSwapIntervalSGI() is actually a redirected call to glXSwapIntervalMESA()!
  if (glXSwapIntervalSGI) {
	  error = glXSwapIntervalSGI(swapInterval);
	  if (error) {
		  if (PsychPrefStateGet_Verbosity()>1) printf("\nPTB-WARNING: FAILED to %s synchronization to vertical retrace!\n\n", (swapInterval > 0) ? "enable" : "disable");
	  }
  }

  // If Mesa query is supported, double-check if the system accepted our settings:
  if (glXGetSwapIntervalMESA) {
	  myinterval = glXGetSwapIntervalMESA();
	  if (myinterval != swapInterval) {
		  if (PsychPrefStateGet_Verbosity()>1) printf("\nPTB-WARNING: FAILED to %s synchronization to vertical retrace (System ignored setting [Req %i != Actual %i])!\n\n", (swapInterval > 0) ? "enable" : "disable", swapInterval, myinterval);
	  }
  }
  
  return;
}

/*
    PsychOSSetGLContext()
    
    Set the window to which GL drawing commands are sent.  
*/
void PsychOSSetGLContext(PsychWindowRecordType *windowRecord)
{
  if (glXGetCurrentContext() != windowRecord->targetSpecific.contextObject) {
    if (glXGetCurrentContext() != NULL) {
      // We need to glFlush the context before switching, otherwise race-conditions may occur:
      glFlush();
      
      // Need to unbind any FBO's in old context before switch, otherwise bad things can happen...
      if (glBindFramebufferEXT) glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    }
    
    // Switch to new context:
    glXMakeCurrent(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, windowRecord->targetSpecific.contextObject);    
  }
}

/*
    PsychOSUnsetGLContext()
    
    Clear the drawing context.  
*/
void PsychOSUnsetGLContext(PsychWindowRecordType* windowRecord)
{
	if (glXGetCurrentContext() != NULL) {
		// We need to glFlush the context before switching, otherwise race-conditions may occur:
		glFlush();
		
		// Need to unbind any FBO's in old context before switch, otherwise bad things can happen...
		if (glBindFramebufferEXT) glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glFlush();
    }
	
	glXMakeCurrent(windowRecord->targetSpecific.deviceContext, None, NULL);
}

/* Same as PsychOSSetGLContext() but for selecting userspace rendering context,
 * optionally copying state from PTBs context.
 */
void PsychOSSetUserGLContext(PsychWindowRecordType *windowRecord, psych_bool copyfromPTBContext)
{
  // Child protection:
  if (windowRecord->targetSpecific.glusercontextObject == NULL) PsychErrorExitMsg(PsychError_user,"GL Userspace context unavailable! Call InitializeMatlabOpenGL *before* Screen('OpenWindow')!");
  
  if (copyfromPTBContext) {
    // This unbind is probably not needed on X11/GLX, but better safe than sorry...
    glXMakeCurrent(windowRecord->targetSpecific.deviceContext, None, NULL);

    // Copy render context state:
    glXCopyContext(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.contextObject, windowRecord->targetSpecific.glusercontextObject, GL_ALL_ATTRIB_BITS);
  }
  
  // Setup new context if it isn't already setup. -> Avoid redundant context switch.
  if (glXGetCurrentContext() != windowRecord->targetSpecific.glusercontextObject) {
    glXMakeCurrent(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, windowRecord->targetSpecific.glusercontextObject);
  }
}

/* PsychOSSetupFrameLock - Check if framelock / swaplock support is available on
 * the given graphics system implementation and try to enable it for the given
 * pair of onscreen windows.
 *
 * If possible, will try to add slaveWindow to the swap group and/or swap barrier
 * of which masterWindow is already a member, putting slaveWindow into a swap-lock
 * with the masterWindow. If masterWindow isn't yet part of a swap group, create a
 * new swap group and attach masterWindow to it, before joining slaveWindow into the
 * new group. If masterWindow is part of a swap group and slaveWindow is NULL, then
 * remove masterWindow from the swap group.
 *
 * The swap lock mechanism used is operating system and GPU dependent. Many systems
 * will not support framelock/swaplock at all.
 *
 * Returns TRUE on success, FALSE on failure.
 */
psych_bool PsychOSSetupFrameLock(PsychWindowRecordType *masterWindow, PsychWindowRecordType *slaveWindow)
{
	GLuint maxGroups, maxBarriers, targetGroup;
	psych_bool rc = FALSE;
	
	// GNU/Linux: Try NV_swap_group support first, then SGI swap group support.

	// NVidia swap group extension supported?
	if((glxewIsSupported("GLX_NV_swap_group") || glewIsSupported("GLX_NV_swap_group")) && (NULL != glXQueryMaxSwapGroupsNV)) {
		// Yes. Check if given GPU really supports it:
		if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: NV_swap_group supported. Querying available groups...\n");

		if (glXQueryMaxSwapGroupsNV(masterWindow->targetSpecific.deviceContext, PsychGetXScreenIdForScreen(masterWindow->screenNumber), &maxGroups, &maxBarriers) && (maxGroups > 0)) {
			// Yes. What to do?
			if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: NV_swap_group supported. Implementation supports up to %i swap groups. Trying to join or unjoin group.\n", maxGroups);

			if (NULL == slaveWindow) {
				// Asked to remove master from swap group:
				glXJoinSwapGroupNV(masterWindow->targetSpecific.deviceContext, masterWindow->targetSpecific.windowHandle, 0);
				masterWindow->swapGroup = 0;
				return(TRUE);
			}
			else {
				// Non-NULL slaveWindow: Shall attach to swap group.
				// Master already part of a swap group?
				if (0 == masterWindow->swapGroup) {
					// Nope. Try to attach it to first available one:
					targetGroup = (GLuint) PsychFindFreeSwapGroupId(maxGroups);
					
					if ((targetGroup == 0) || !glXJoinSwapGroupNV(masterWindow->targetSpecific.deviceContext, masterWindow->targetSpecific.windowHandle, targetGroup)) {
						// Failed!
						if (PsychPrefStateGet_Verbosity() > 1) {
							printf("PTB-WARNING: Tried to enable framelock support for master-slave window pair, but masterWindow failed to join swapgroup %i! Skipped.\n", targetGroup);
						}
						
						goto try_sgi_swapgroup;
					}
					
					// Sucess for master!
					masterWindow->swapGroup = targetGroup;
				}
				
				// Now try to join the masters swapgroup with the slave:
				if (!glXJoinSwapGroupNV(slaveWindow->targetSpecific.deviceContext, slaveWindow->targetSpecific.windowHandle,  masterWindow->swapGroup)) {
					// Failed!
					if (PsychPrefStateGet_Verbosity() > 1) {
						printf("PTB-WARNING: Tried to enable framelock support for master-slave window pair, but slaveWindow failed to join swapgroup %i of master! Skipped.\n", masterWindow->swapGroup);
					}
					
					goto try_sgi_swapgroup;
				}
				
				// Success! Now both windows are in a common swapgroup and framelock should work!
				slaveWindow->swapGroup = masterWindow->swapGroup;
				
				if (PsychPrefStateGet_Verbosity() > 1) {
					printf("PTB-INFO: Framelock support for master-slave window pair via NV_swap_group extension enabled! Joined swap group %i.\n", masterWindow->swapGroup);
				}
				
				return(TRUE);
			}
		}
	}

// If we reach this point, then NV_swap groups are unsupported, or setup failed.
try_sgi_swapgroup:

	// Try if we have more luck with SGIX_swap_group extension:
	if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: NV_swap_group unsupported or join operation failed. Trying GLX_SGIX_swap_group support...\n");

	// SGIX swap group extension supported?
	if((glxewIsSupported("GLX_SGIX_swap_group") || glewIsSupported("GLX_SGIX_swap_group")) && (NULL != glXJoinSwapGroupSGIX)) {
		// Yes. What to do?
		if (NULL == slaveWindow) {
			// Asked to remove master from swap group:
			glXJoinSwapGroupSGIX(masterWindow->targetSpecific.deviceContext, masterWindow->targetSpecific.windowHandle, None);
			masterWindow->swapGroup = 0;
			return(TRUE);
		}
		else {
			// Non-NULL slaveWindow: Shall attach to swap group.

			// Sucess for master by definition. Master is member of its own swapgroup, obviously...
			masterWindow->swapGroup = 1;
			
			// Now try to join the masters swapgroup with the slave. This can't fail in a non-fatal way.
			// Either it succeeds, or the whole runtime will abort with some GLX command stream error :-I
			glXJoinSwapGroupSGIX(slaveWindow->targetSpecific.deviceContext, slaveWindow->targetSpecific.windowHandle,  masterWindow->targetSpecific.windowHandle);
			
			// Success! Now both windows are in a common swapgroup and framelock should work!
			slaveWindow->swapGroup = masterWindow->swapGroup;
			
			if (PsychPrefStateGet_Verbosity() > 1) {
				printf("PTB-INFO: Framelock support for master-slave window pair via GLX_SGIX_swap_group extension enabled!\n");
			}
			
			return(TRUE);
		}
	}
	
	if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: NV_swap_group and GLX_SGIX_swap_group unsupported or join operations failed.\n");
	
	return(rc);
}

// Perform OS specific processing of Window events:
void PsychOSProcessEvents(PsychWindowRecordType *windowRecord, int flags)
{
	Window rootRet;
	unsigned int depth_return, border_width_return, w, h;
	int x, y;

	// Trigger event queue dispatch processing for GUI windows:
	if (windowRecord == NULL) {
		// No op, so far...
		return;
	}
	
	// GUI windows need to behave GUIyee:
	if ((windowRecord->specialflags & kPsychGUIWindow) && PsychIsOnscreenWindow(windowRecord)) {
		// Update windows rect and globalrect, based on current size and location:
		XGetGeometry(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, &rootRet, &x, &y,
			     &w, &h, &border_width_return, &depth_return);
		XTranslateCoordinates(windowRecord->targetSpecific.deviceContext, windowRecord->targetSpecific.windowHandle, rootRet,
				      0,0, &x, &y, &rootRet);
		PsychMakeRect(windowRecord->globalrect, x, y, x + (int) w - 1, y + (int) h - 1);
		PsychNormalizeRect(windowRecord->globalrect, windowRecord->rect);
		PsychSetupView(windowRecord);
	}
}
