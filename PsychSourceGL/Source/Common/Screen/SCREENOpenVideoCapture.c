/*
  Psychtoolbox3/Source/Common/SCREENOpenVideoCapture.c		
  
  AUTHORS:
    mario.kleiner at tuebingen.mpg.de   mk
  
  PLATFORMS:	
  This file should build on any platform. 

  HISTORY:
  2/7/06  mk		Created. 
 
  DESCRIPTION:
  
  Open a video capture source (framegrabber, webcam, ...), create and initialize a corresponding capture object
  and return a handle to it to MATLAB space.
 
  On OS-X and Windows, video capture is implemented via Apples Sequence-Grabber API which is part of Quicktime.
  On Linux, we support Firewire machine vision cameras that comply to the IIDC-1.x specification.
  
  TO DO:
  
  
  

*/


#include "Screen.h"


static char useString[] = "videoPtr = Screen('OpenVideoCapture', windowPtr [, deviceIndex] [,roirectangle] [, pixeldepth] [, numbuffers] [, allowfallback]);";
static char synopsisString[] = 
	"Try to open the video source 'deviceIndex' for video capture into onscreen window 'windowPtr' and "
        "return a handle 'videoPtr' on success. If 'deviceIndex' is left out, it defaults to zero - use the "
        "first capture device attached to your machine. 'roirectangle' if specified, defines the requested size "
        "for captured images. The default is to return the maximum size image provided by the capture device. "
        "The real ROI (region of interest) may differ from the requested one, depending on the capabilities of "
        "your capture device. 'pixeldepth' if provided, asks for the number of layers that captured textures "
        "should have: 1=Luminance image, 2=Luminance+Alpha image, 3=RGB image, 4=RGB+Alpha image. Default is "
        "to take whatever the capture device provides by default. Different devices support different formats "
        "so some of these settings may be ignored. 'numbuffers' if provided, specifies the number of internal "
        "video buffers to use. It defaults to a value that is optimal for your specific hardware for common use. "
        "'allowfallback' if set to 1, will allow Psychtoolbox to use a less efficient mode of operation for video "
        "capture if your specific hardware or operating system setup doesn't allow to use the high-performance "
        "mode. 'allowfallback' defaults to 1 = Allow fallback path. On OS-X and Windows, video capture is handled by use of "
        "Apples Quicktime Sequence-Grabber API. Linux currently only supports high performance machine vision cameras that "
        "are compliant with the IIDC-1.x standard and that are connected via a Firewire (IEEE1394) bus system. Use of Linux "
        "with such cams allows for much higher flexibility and performance than use of video capture on OS-X or Windows. ";

static char seeAlsoString[] = "CloseVideoCapture StartVideoCapture StopVideoCapture GetCapturedImage";

PsychError SCREENOpenVideoCapture(void) 
{
        PsychWindowRecordType			*windowRecord;
        int                                     deviceIndex;
        int                                     capturehandle = -1;
        double                                  framerate;
        int                                     width;
        int                                     height;
        PsychRectType                           roirectangle;
        Boolean                                 roiassigned;
	int                                     reqdepth = 0;
	int                                     num_dmabuffers = 0;
	int                                     allow_lowperf_fallback = 1;

	// All sub functions should have these two lines
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none);};

        PsychErrorExit(PsychCapNumInputArgs(6));            // Max. 6 input args.
        PsychErrorExit(PsychRequireNumInputArgs(1));        // Min. 1 input args required.
        PsychErrorExit(PsychCapNumOutputArgs(1));           // Max. 1 output args.
        
        // Get the window record from the window record argument and get info from the window record
        PsychAllocInWindowRecordArg(kPsychUseDefaultArgPosition, TRUE, &windowRecord);
        // Only onscreen windows allowed:
        if(!PsychIsOnscreenWindow(windowRecord)) {
            PsychErrorExitMsg(PsychError_user, "OpenVideoCapture called on something else than an onscreen window.");
        }
        
        // Get the device index. We default to the first device if none is given:
        deviceIndex=0;
        PsychCopyInIntegerArg(2, FALSE, &deviceIndex);

        // Get the optional roi rectangle:
        roiassigned = PsychCopyInRectArg(3, FALSE, roirectangle);
	if (IsPsychRectEmpty(roirectangle)) PsychErrorExitMsg(PsychError_user, "OpenVideoCapture called with invalid (empty) roirectangle argument.");


	// Query (optional) output texture pixel depth: By default, we take whatever we get from the capture device:
        PsychCopyInIntegerArg(4, FALSE, &reqdepth);
	if (reqdepth<0) PsychErrorExitMsg(PsychError_user, "OpenVideoCapture called with invalid (negative) pixeldepth argument.");
	// Query number of ringbuffers to use. Our default is coded in the OS dependent subroutines.
        PsychCopyInIntegerArg(5, FALSE, &num_dmabuffers);
	if (num_dmabuffers<0) PsychErrorExitMsg(PsychError_user, "OpenVideoCapture called with invalid (negative) numbuffers argument.");
	// Query, if use of low-performance fallback code is allowed if high-perf engine fails:
        PsychCopyInIntegerArg(6, FALSE, &allow_lowperf_fallback);
	if (allow_lowperf_fallback<0) PsychErrorExitMsg(PsychError_user, "OpenVideoCapture called with invalid (negative) allowfallback flag.");

	// Try to open the capture device and create & initialize a corresponding capture object.
        // A MATLAB handle to the video capture object is returned upon successfull operation.
        if (roiassigned) {
            PsychOpenVideoCaptureDevice(windowRecord, deviceIndex, &capturehandle, roirectangle, reqdepth, num_dmabuffers, allow_lowperf_fallback);
        }
        else {
            PsychOpenVideoCaptureDevice(windowRecord, deviceIndex, &capturehandle, NULL, reqdepth, num_dmabuffers, allow_lowperf_fallback);
        }
        
        // Upon sucessfull completion, we'll have a valid handle in 'capturehandle'. Return it to Matlab-world:
        PsychCopyOutDoubleArg(1, TRUE, (double) capturehandle);

	// Ready!
        return(PsychError_none);
}

