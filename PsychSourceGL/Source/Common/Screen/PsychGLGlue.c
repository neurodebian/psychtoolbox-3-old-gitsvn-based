/*
	PsychToolbox2/Source/Common/Screen/PsychGLGlue.c
	
	PLATFORMS:	Windows
				MacOS9
			
	
	AUTHORS:
	Allen Ingling		awi		Allen.Ingling@nyu.edu

	HISTORY:
	09/09/02			awi		wrote it.  
	
	DESCRIPTION:
	
	Functions to convert between Psych toolbox data types and GL data types.  	
        
        TO DO:
        
*/

#include "Screen.h"

/*
    PsychConvertColorToDoubleVector()
    
    Accept a color structure and a screen depth and return either three or four double values in the range between
    0-1 which specify r, g, b and optinally alpha values.
    
    The value array argument should be be four elements long.
    
*/
int PsychConvertColorToDoubleVector(PsychColorType *color, PsychWindowRecordType *windowRecord, GLdouble *valueArray)
{
	GLdouble deno;
    
	// Read denominator from windowRecord. Need to get rid of the sign, because it
	// encodes if we have color clamping enabled or not:
	deno = fabs(windowRecord->colorRange);
	
    switch(color->mode){
        case kPsychIndexColor:
            valueArray[0]=color->value.index.i/deno;
            return(1);
        case kPsychRGBColor:
            valueArray[0]=color->value.rgb.r/deno;
            valueArray[1]=color->value.rgb.g/deno;
            valueArray[2]=color->value.rgb.b/deno;
            return(3); 
        case kPsychRGBAColor:
            valueArray[0]=color->value.rgba.r/deno;
            valueArray[1]=color->value.rgba.g/deno;
            valueArray[2]=color->value.rgba.b/deno;
            valueArray[3]=(color->value.rgba.a == DBL_MAX) ? 1.0 : color->value.rgba.a/deno;
            return(4);
        case kPsychUnknownColor:
            PsychErrorExitMsg(PsychError_internal,"Unspecified display mode");
    }
    PsychErrorExitMsg(PsychError_internal,"Unknown display mode");
    return(0); //make the compiler happy.  
}

/*
    PsychSetGLColor()
    
    Accept a Psych color structure and a depth value and call the appropriate variant of glColor.       
*/
void PsychSetGLColor(PsychColorType *color, PsychWindowRecordType *windowRecord)
{
    GLdouble dVals[4]; 
    int numVals;
    
    numVals=PsychConvertColorToDoubleVector(color, windowRecord, dVals);
    if(numVals==1)
        PsychErrorExitMsg(PsychError_internal, "palette mode not yet implemented");
    else if(numVals==3)
        glColor3dv(dVals);
    else if(numVals==4)
        glColor4dv(dVals);
    else
        PsychErrorExitMsg(PsychError_internal, "Illegal color specifier"); 
}

/*
    PsychGLRect()
*/
void PsychGLRect(double *psychRect)
{
    glRectd((GLdouble)(psychRect[kPsychLeft]),
            (GLdouble)(psychRect[kPsychTop]),
            (GLdouble)(psychRect[kPsychRight]),
            (GLdouble)(psychRect[kPsychBottom]));
}

char *PsychGetGLErrorNameString(GLenum errorConstant)
{
    static char GL_NO_ERROR_str[] = "GL_NO_ERROR";
    static char GL_INVALID_ENUM_str[] = "GL_INVALID_ENUM";
    static char GL_INVALID_VALUE_str[] = "GL_INVALID_VALUE";
    static char GL_INVALID_OPERATION_str[] = "GL_INVALID_OPERATION";
    static char GL_STACK_OVERFLOW_str[] = "GL_STACK_OVERFLOW";
    static char GL_STACK_UNDERFLOW_str[] = "GL_STACK_UNDERFLOW";
    static char GL_OUT_OF_MEMORY_str[] = "GL_OUT_OF_MEMORY";
    static char GL_TABLE_TOO_LARGE_str[] = "GL_TABLE_TOO_LARGE";
    static char unrecognized_error_str[] = "unrecognized GL error constant";
    
    switch(errorConstant){
        case GL_NO_ERROR: return(GL_NO_ERROR_str);
        case GL_INVALID_ENUM: return(GL_INVALID_ENUM_str);
        case GL_INVALID_VALUE: return(GL_INVALID_VALUE_str);
        case GL_INVALID_OPERATION: return(GL_INVALID_OPERATION_str);
        case GL_STACK_OVERFLOW: return(GL_STACK_OVERFLOW_str);
        case GL_STACK_UNDERFLOW: return(GL_STACK_UNDERFLOW_str);
        case GL_OUT_OF_MEMORY: return(GL_OUT_OF_MEMORY_str);
        case GL_TABLE_TOO_LARGE: return(GL_TABLE_TOO_LARGE_str);
    }
    return(unrecognized_error_str);
           
}

/*
	PsychGetGLErrorListString()
	
*/ 
Boolean PsychGetGLErrorListString(const char **errorListStr)
{
	#define MAX_GL_ERROR_LIST_LEN			2048
	#define MAX_GL_ERROR_LIST_DELTA_LEN		256
	static char	errorListString[MAX_GL_ERROR_LIST_LEN];
	char	*errorNameStr;
	int		currentIndex, deltaStrLen, nextCurrentIndex;
    GLenum	glError;
    boolean	isError=FALSE;
	
	currentIndex=0;
    for(glError=glGetError(); glError!=GL_NO_ERROR; glError=glGetError()){
		errorNameStr=PsychGetGLErrorNameString(glError);
		deltaStrLen=strlen(errorNameStr)+2;  //2 chars: comma and space
		nextCurrentIndex=currentIndex+deltaStrLen;
		if(nextCurrentIndex >= MAX_GL_ERROR_LIST_LEN)
			PsychErrorExitMsg(PsychError_internal,"string memory overflow");
		if(isError)
			sprintf(&(errorListString[currentIndex]), " ,%s", errorNameStr);
		else
			sprintf(&(errorListString[currentIndex]), "%s", errorNameStr);
		currentIndex=nextCurrentIndex;
		isError=TRUE;		
	}
	if(isError)
		*errorListStr=errorListString;
	else
		*errorListStr=NULL;
	return(isError);
}

void PsychTestForGLErrorsC(int lineNum, const char *funcName, const char *fileName)
{
    boolean			isError;
	const char		*glErrorListString;
    
	isError=PsychGetGLErrorListString(&glErrorListString);
	if(isError)
		PsychErrorExitC(PsychError_OpenGL, 
						glErrorListString, 
						lineNum, 
						funcName, 
						fileName);
}

/*
	PsychExtractQuadVertexFromRect()
	
	Return one of the four vertices define by a Psych rect in a 2-element array of GLdoubles.
	Vertices are numbered from the top left corner (0) clockwise to the bottom left corner (3).
*/
GLdouble *PsychExtractQuadVertexFromRect(double *rect, int vertexNumber, GLdouble *vertex)
{
	switch(vertexNumber){
		case 0:
			vertex[0]=(GLdouble)rect[0];
			vertex[1]=(GLdouble)rect[1];
			break;
		case 1:
			vertex[0]=(GLdouble)rect[2];
			vertex[1]=(GLdouble)rect[1];
			break;
		case 2:
			vertex[0]=(GLdouble)rect[2];
			vertex[1]=(GLdouble)rect[3];
			break;
		case 3:
			vertex[0]=(GLdouble)rect[0];
			vertex[1]=(GLdouble)rect[3];
			break;
		default:
			PsychErrorExitMsg(PsychError_internal, "Illegal vertex value");
	}
	return(vertex);
}

/* PsychPrepareRenderBatch()
 *
 * Perform setup for a batch of render requests for a specific primitive. Some 2D Screen
 * drawing commands allow to specify a list of primitives to draw instead of only a single
 * one. E.g. 'DrawDots' allows to draw thousands of dots with one single DrawDots command.
 * This helper routine is called by such batch-capable commands. It checks which input arguments
 * are provided and if its a single one or multiple ones. It sets up the rendering pipe accordingly,
 * performing required conversion steps. The actual drawing routine just needs to perform primitive
 * specific code.
 */
void PsychPrepareRenderBatch(PsychWindowRecordType *windowRecord, int coords_pos, int* coords_count, double** xy, int colors_pos, int* colors_count, int* colorcomponent_count, double** colors, unsigned char** bytecolors, int sizes_pos, int* sizes_count, double** size)
{
	PsychColorType							color;
	int                                     whiteValue, m,n,p,mc,nc,pc,idot_type;
	int                                     i, nrpoints, nrsize;
	boolean                                 isArgThere, isdoublecolors, isuint8colors, usecolorvector, needxy;
	double									*tmpcolors, *pcolors, *tcolors;
	double									convfactor;

	needxy = (coords_pos > 0) ? TRUE: FALSE;
	coords_pos = abs(coords_pos);
	colors_pos = abs(colors_pos);
	sizes_pos = abs(sizes_pos);
	
	// Get mandatory or optional xy coordinates argument
	isArgThere = PsychIsArgPresent(PsychArgIn, coords_pos);
	if(!isArgThere && needxy) {
		PsychErrorExitMsg(PsychError_user, "No position argument supplied");
	}
	
	if (isArgThere) {
		PsychAllocInDoubleMatArg(coords_pos, TRUE, &m, &n, &p, xy);
		if(p!=1 || m!=*coords_count) {
			printf("PTB-ERROR: Coordinates must be a %i tuple or a %i rows vector.\n", *coords_count, *coords_count);
			PsychErrorExitMsg(PsychError_user, "Invalid format for coordinate specification.");
		}
		
		nrpoints=n;
		*coords_count = n;
	}
	else {
		nrpoints = 0;
		*coords_count = 0;
	}
	
	if (size) {
		// Get optional size argument
		isArgThere = PsychIsArgPresent(PsychArgIn, sizes_pos);
		if(!isArgThere){
			// No size provided: Use a default size of 1.0:
			*size = (double *) PsychMallocTemp(sizeof(double));
			*size[0] = 1;
			nrsize=1;
		} else {
			PsychAllocInDoubleMatArg(3, TRUE, &m, &n, &p, size);
			if(p!=1) PsychErrorExitMsg(PsychError_user, "Size must be a scalar or a vector with one column or row");
			nrsize=m*n;
			if (nrsize!=nrpoints && nrsize!=1 && *sizes_count!=1) PsychErrorExitMsg(PsychError_user, "Size vector must contain one size value per item.");
		}
		
		*sizes_count = nrsize;
	}	

	// Check if color argument is provided:
	isArgThere = PsychIsArgPresent(PsychArgIn, colors_pos);        
	if(!isArgThere) {
		// No color argument provided - Use defaults:
		whiteValue=PsychGetWhiteValueFromWindow(windowRecord);
		PsychLoadColorStruct(&color, kPsychIndexColor, whiteValue ); //index mode will coerce to any other.
		usecolorvector=false;
	}
	else {
		// Some color argument provided. Check first, if it's a valid color vector:
		isdoublecolors = PsychAllocInDoubleMatArg(colors_pos, kPsychArgAnything, &mc, &nc, &pc, colors);
		isuint8colors  = PsychAllocInUnsignedByteMatArg(colors_pos, kPsychArgAnything, &mc, &nc, &pc, bytecolors);
		
		// Do we have a color vector, aka one element per vertex?
		if((isdoublecolors || isuint8colors) && pc==1 && nc==nrpoints && nrpoints>1) {
			// Looks like we might have a color vector... ... Double-check it:
			if (mc!=3 && mc!=4) PsychErrorExitMsg(PsychError_user, "Color vector must be a 3 or 4 row vector");
			// Yes. colors is a valid pointer to it.
			usecolorvector=true;
			
			if (isdoublecolors) {
				if (fabs(windowRecord->colorRange)!=1) {
					// We have to loop through the vector and divide all values by windowRecord->colorRange, so the input values
					// 0-colorRange get mapped to the range 0.0-1.0, as OpenGL expects values in range 0-1 when
					// a color vector is passed in Double- or Float format.
					// This is inefficient, as it burns some cpu-cycles, but necessary to keep color
					// specifications consistent in the PTB - API.
					convfactor = 1.0 / fabs(windowRecord->colorRange);
					tmpcolors=PsychMallocTemp(sizeof(double) * nc * mc);
					pcolors = *colors;
					tcolors = tmpcolors;
					for (i=0; i<(nc*mc); i++) {
						*(tcolors++)=(*pcolors++) * convfactor;
					}
				}
				else {
					// colorRange is == 1 --> No remapping needed as colors are already in proper range!
					// Just setup pointer to our unaltered input color vector:
					tmpcolors=*colors;
				}
				
				*colors = tmpcolors;
			}
			else {
				// Color vector in uint8 format. Nothing to do.
			}
		}
		else {
			// No color vector provided: Check for a single valid color triplet or quadruple:
			usecolorvector=false;
			isArgThere=PsychCopyInColorArg(colors_pos, TRUE, &color);                
		}
	}
	
	// Enable rendering context for windowRecord:
	PsychSetGLContext(windowRecord);
	
	// Enable this windowRecords framebuffer as current drawingtarget:
	PsychSetDrawingTarget(windowRecord);
	
	// Setup alpha blending properly:
	PsychUpdateAlphaBlendingFactorLazily(windowRecord);
	
 	// Setup common color for all objects if no color vector has been provided:
	if (!usecolorvector) {
		PsychCoerceColorMode(&color);
		PsychSetGLColor(&color, windowRecord);
		*colors_count = 1;
	}
	else {
		*colors_count = nc;
	}
	*colorcomponent_count = mc;
		
	return;
}
