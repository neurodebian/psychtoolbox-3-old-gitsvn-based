/*
    SCREENFrameOval.c		
  
    AUTHORS:
    
		Allen.Ingling@nyu.edu		awi 
  
    PLATFORMS:
	
		Only OS X for now.
    
    
    HISTORY:
	
		mm/dd/yy	
		
		10/10/03	awi		Created.  Based on FillOval.
		10/12/04	awi		In useString: changed "SCREEN" to "Screen", and moved commas to inside [].
		1/15/05		awi		Removed GL_BLEND setting a MK's suggestion. 
		1/25/05		awi		Really removed GL_BLEND.  Correction provide by mk.
		2/25/05		awi		Added call to PsychUpdateAlphaBlendingFactorLazily().  Drawing now obeys settings by Screen('BlendFunction').
		

    
    TO DO:
    
    BUGS:  
    
		The pen width is not uniform along the oval if it is elongated.  This is because we scale the line thickness with the oval.
		Here are some ideas about how to fix this:
			-The OS9 Psychtoolbox used drawing commands which mapped directly onto QuickDraw commands.  The same drawing commands are a bad fit for OpenGL.
			After version 1.0 we should migrate the Psychtoolbox to a new set of drawing commands which map closely onto GL commands.  Meanwhile, for backwards
			compatability with older scripts, we might consider implementing the old drawing commands with actual QuickDraw or perhaps Quartz calls.  See 
			Apple's QA1011 for how to combine QuickDraw with CGDirectDisplay:
			http://developer.apple.com/qa/qa2001/qa1011.html

			-We could composite an oval by drawing a smaller background oval within a larger forground oval and setting alpha on the smaller oval to transparent and
			on the larger oval to opaque. The problem with this is that we actally have to allocate a compositing window since, unless there is some clever way to 
			do this in the destination window without disturbing he existing image, by careful selectin of copy modes.
			
			-We could implement (or borrow) an oval drawing routine.  This seems like a bad solution.     
  
*/


#include "Screen.h"

// If you change useString then also change the corresponding synopsis string in ScreenSynopsis.c
static char useString[] = "Screen('FrameOval', windowPtr [,color] [,rect] [,penWidth] [,penHeight] [,penMode]);";
//                                             1           2        3      4            5            6
static char synopsisString[] = 
            "Draw the outline of an oval.  \"color\" is the clut index (scalar or [r g b] "
            "triplet) that you want to poke into each pixel; default produces black with the "
            "standard CLUT for this window's pixelSize. Default \"rect\" is entire window. "
            "Default pen size is 1,1.  OSX: The penMode argumenet is ignored.  The penWidth must "
            "equal the penHeight.  If non-equal arguments are given FrameOval will choose the maximum "
            "value.  The pen width will be non-uniform for non-circular ovals, this is a bug.";
static char seeAlsoString[] = "FillOval";	
            
PsychError SCREENFrameOval(void)  
{
	
	PsychColorType 		color;
	PsychRectType 		rect;
	double			numSlices, outerRadius, xScale, yScale, xTranslate, yTranslate, rectY, rectX, penWidth, penHeight, penSize, innerRadius;
	PsychWindowRecordType	*windowRecord;
	int 			depthValue, whiteValue, colorPlaneSize, numColorPlanes;
	boolean 		isArgThere;
	GLUquadricObj		*diskQuadric;
    
	//all sub functions should have these two lines
	PsychPushHelp(useString, synopsisString,seeAlsoString);
	if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);}
	
	//check for superfluous arguments
	PsychErrorExit(PsychCapNumInputArgs(6));   //The maximum number of inputs
	PsychErrorExit(PsychCapNumOutputArgs(0));  //The maximum number of outputs

	//get the window record from the window record argument and get info from the window record
	PsychAllocInWindowRecordArg(kPsychUseDefaultArgPosition, TRUE, &windowRecord);
        
	//Get the depth from the window, we need this to interpret the color argument.
	depthValue=PsychGetWindowDepthValueFromWindowRecord(windowRecord);
	numColorPlanes=PsychGetNumPlanesFromDepthValue(depthValue);
	colorPlaneSize=PsychGetColorSizeFromDepthValue(depthValue);

	//Get the color argument or use the default, then coerce to the form determened by the window depth.  
	isArgThere=PsychCopyInColorArg(kPsychUseDefaultArgPosition, FALSE, &color);
		if(!isArgThere){
			whiteValue=PsychGetWhiteValueFromDepthValue(depthValue);
			PsychLoadColorStruct(&color, kPsychIndexColor, whiteValue ); //index mode will coerce to any other.
		}
 	PsychCoerceColorModeFromSizes(numColorPlanes, colorPlaneSize, &color);
        

	//get the rect value
	isArgThere=PsychCopyInRectArg(kPsychUseDefaultArgPosition, FALSE, rect);
	if(!isArgThere) PsychCopyRect(rect, windowRecord->rect);
	if (IsRectEmpty(rect)) return(PsychError_none);
            
	//get the pen width and height arguments
	penWidth=1;
	penHeight=1;
	PsychCopyInDoubleArg(4, FALSE, &penWidth);
	PsychCopyInDoubleArg(5, FALSE, &penHeight);
	penSize= penWidth > penHeight ? penWidth : penHeight;

	//The glu disk object location and size with a  center point and a radius,   
	//whereas FillOval accepts a bounding rect.   Converting from one set of parameters
	//to the other we should careful what we do for rects size of even number of pixels in length.
	PsychGetCenterFromRectAbsolute(rect, &xTranslate, &yTranslate);
	rectY=PsychGetHeightFromRect(rect);
	rectX=PsychGetWidthFromRect(rect);
	if(rectX == rectY){
		xScale=1; 
		yScale=1;
		outerRadius=rectX/2;
	}else if(rectX > rectY){ 
		xScale=1;
		yScale=rectY/rectX;
		outerRadius=rectX/2;
	}else if(rectY > rectX){
		yScale=1;
		xScale=rectX/rectY;
		outerRadius=rectY/2;
	}
	numSlices=3.14159265358979323846  * 2 * outerRadius;
	innerRadius=outerRadius- 2*penSize;
	innerRadius= innerRadius < 0 ? 0 : innerRadius;         
	
	//Set the context & color
	PsychSetGLContext(windowRecord);
        // Enable this windowRecords framebuffer as current drawingtarget:
        PsychSetDrawingTarget(windowRecord);

	PsychUpdateAlphaBlendingFactorLazily(windowRecord);
	PsychSetGLColor(&color, depthValue);
	//glEnable(GL_POLYGON_SMOOTH);
        //Draw the rect.  
	glPushMatrix();
		glTranslated(xTranslate,yTranslate,0);
		glScaled(xScale, yScale, 1);
		diskQuadric=gluNewQuadric();
		gluDisk(diskQuadric, innerRadius, outerRadius, numSlices, 1);
		gluDeleteQuadric(diskQuadric);
	glPopMatrix();

        // Mark end of drawing op. This is needed for single buffered drawing:
        PsychFlushGL(windowRecord);

 	//All psychfunctions require this.
	return(PsychError_none);
}




