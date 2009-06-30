/*
	SCREENDrawLine.c		

	AUTHORS:

	Allen.Ingling@nyu.edu		awi 

	PLATFORMS:	

	All.

	HISTORY:

		07/23/04	awi		Created.   
		10/12/04	awi		In useString: moved commas to inside [].
		11/16/04    awi		Fixed a bug in DrawLine where dX should have been dY.
		2/25/05		awi		Added call to PsychUpdateAlphaBlendingFactorLazily().  Drawing now obeys settings by Screen('BlendFunction').

	TO DO:

*/


#include "Screen.h"

// If you change useString then also change the corresponding synopsis string in ScreenSynopsis.c
static char useString[] = "Screen('DrawLine', windowPtr [,color], fromH, fromV, toH, toV [,penWidth]);";
//                                            1           2       3      4      5    6     7          
static char synopsisString[] = 
	"Draw the outline of a rectangle. \"color\" is the clut index (scalar or [r g b a] "
    "vector) that you want to poke into each pixel; default produces black "
    "Default \"rect\" is entire window. "
    "Default pen size is 1. ";
	
static char seeAlsoString[] = "FrameRect";	

PsychError SCREENDrawLine(void)  
{
	
	PsychColorType					color;
	PsychWindowRecordType			*windowRecord;
	int								whiteValue;
	psych_bool							isArgThere;
	double							sX, sY, dX, dY, penSize;
    
	//all sub functions should have these two lines
	PsychPushHelp(useString, synopsisString,seeAlsoString);
	if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};
	
	//check for superfluous arguments
	PsychErrorExit(PsychCapNumInputArgs(7));   //The maximum number of inputs
	PsychErrorExit(PsychCapNumOutputArgs(0));  //The maximum number of outputs

	//get the window record from the window record argument and get info from the window record
	PsychAllocInWindowRecordArg(1, kPsychArgRequired, &windowRecord);
	
	//Get the color argument or use the default, then coerce to the form determened by the window depth.  
	isArgThere=PsychCopyInColorArg(2, FALSE, &color);
	if(!isArgThere){
		whiteValue=PsychGetWhiteValueFromWindow(windowRecord);
		PsychLoadColorStruct(&color, kPsychIndexColor, whiteValue ); //index mode will coerce to any other.
	}

 	PsychCoerceColorMode( &color);
        
	//get source and destination X and Y values
	PsychCopyInDoubleArg(3, kPsychArgRequired, &sX);
	PsychCopyInDoubleArg(4, kPsychArgRequired, &sY);
	PsychCopyInDoubleArg(5, kPsychArgRequired, &dX);
	PsychCopyInDoubleArg(6, kPsychArgRequired, &dY);
	
	//get and set the pen size
	penSize=1;
	PsychCopyInDoubleArg(7, kPsychArgOptional, &penSize);
	
	// Enable this windowRecords framebuffer as current drawingtarget:
	PsychSetDrawingTarget(windowRecord);

	// Set default draw shader:
	PsychSetShader(windowRecord, -1);

	glLineWidth((GLfloat)penSize);

	PsychUpdateAlphaBlendingFactorLazily(windowRecord);
	PsychSetGLColor(&color, windowRecord);
	glBegin(GL_LINES);
		glVertex2d((GLdouble)sX, (GLdouble)sY);
		glVertex2d((GLdouble)dX, (GLdouble)dY);
	glEnd();
	
	glLineWidth((GLfloat) 1);

	// Mark end of drawing op. This is needed for single buffered drawing:
	PsychFlushGL(windowRecord);

	return(PsychError_none);
}
