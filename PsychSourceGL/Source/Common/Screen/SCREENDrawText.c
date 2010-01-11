/*
    SCREENDrawText.c	
  
    AUTHORS:
    
		Allen.Ingling@nyu.edu				awi
		mario.kleiner at tuebingen.mpg.de	mk
  
    PLATFORMS:
		
		All. With OS specific #ifdefs...
    
    HISTORY:
	
		11/17/03	awi		Spun off from SCREENTestTexture which also used Quartz and Textures to draw text but did not match the 'DrawText' specifications.
		10/12/04	awi		In useString: changed "SCREEN" to "Screen", and moved commas to inside [].
		2/25/05		awi		Added call to PsychUpdateAlphaBlendingFactorLazily().  Drawing now obeys settings by Screen('BlendFunction').
		5/08/05     mk      Bugfix for "Descenders of letters get cut/eaten away" bug introduced in PTB 1.0.5
		10/12/05    mk      Fix crash in DrawText caused by removing glFinish() while CLIENT_STORAGE is enabled!
							-> Disabling CLIENT_STORAGE and removing glFinish() is the proper solution...
		11/01/05    mk      Finally the real bugfix for "Descenders of letters get cut/eaten away" bug introduced in PTB 1.0.5!
		11/01/05    mk      Removal of dead code + beautification.
		11/21/05    mk      Code for updating the "Drawing Cursor" and returning NewX, NewY values added.
		01/01/06    mk      Code branch for M$-Windoze implementation of DrawText added.
		11/11/07	mk		New GDI based Windoze text renderer implemented.
		12/27/09	mk		Massive refactoring of code for all platforms and support for plugin-based textrenderers.
							-> Cleans up the mess of duplicated code and special cases. We share as much code as possible accross platforms.
							-> Allows for unicode support on all platforms.
							-> Allows for plugin renderer on Linux and OS/X for unicode, anti-aliasing etc.
							-> Allows for better handling of unicode and multibyte character encodings.

    DESCRIPTION:

		Unified file with text renderers for all platforms (OS/X, Windows, Linux).
  
    REFERENCES:
	
		http://oss.sgi.com/projects/ogl-sample/registry/APPLE/client_storage.txt
		http://developer.apple.com/samplecode/Sample_Code/Graphics_3D/TextureRange.htm
		
		http://www.cl.cam.ac.uk/~mgk25/unicode.html - A good FAQ about Unicode, UTF-8 with a special emphasis on Linux and Posix systems.

    TO DO:

		Platform specific code should be in the platform folders, not here in the Common folder! Sort this out some time.
							
*/


#include "Screen.h"

// Reference to external dynamically loaded text renderer plugin:
static void* drawtext_plugin = NULL;
static psych_bool drawtext_plugin_firstcall = TRUE;

// Function prototypes for functions exported by drawtext plugins: Will be dynamically bound & linked:
int (*PsychPluginInitText)(void) = NULL;
int (*PsychPluginShutdownText)(void) = NULL;
int (*PsychPluginRebuiltFont)(void) = NULL;
int (*PsychPluginSetTextFont)(const char* fontName) = NULL;
int (*PsychPluginSetTextStyle)(unsigned int fontStyle) = NULL;
int (*PsychPluginSetTextSize)(double fontSize) = NULL;
void (*PsychPluginSetTextFGColor)(double* color) = NULL;
void (*PsychPluginSetTextBGColor)(double* color) = NULL;
void (*PsychPluginSetTextUseFontmapper)(unsigned int useMapper, unsigned int mapperFlags) = NULL;
void (*PsychPluginSetTextViewPort)(double xs, double ys, double w, double h) = NULL;
int (*PsychPluginDrawText)(double xStart, double yStart, int textLen, double* text) = NULL;
int (*PsychPluginMeasureText)(int textLen, double* text, float* xmin, float* ymin, float* xmax, float* ymax) = NULL;
void (*PsychPluginSetTextVerbosity)(unsigned int verbosity) = NULL;
void (*PsychPluginSetTextAntiAliasing)(int antiAliasing) = NULL;

// External renderplugins not yet supported on MS-Windows:
#if PSYCH_SYSTEM != PSYCH_WINDOWS
// Include for dynamic loading of external renderplugin:
#include <dlfcn.h>
#endif

// If you change useString then also change the corresponding synopsis string in ScreenSynopsis.
static char useString[] = "[newX,newY]=Screen('DrawText', windowPtr, text [,x] [,y] [,color] [,backgroundColor] [,yPositionIsBaseline] [,swapTextDirection]);";
//							1	 2						  1			 2		3	 4	  5		   6				  7						8

// Synopsis string for DrawText:
static char synopsisString[] = 
    "Draw text. \"text\" may include Unicode characters (e.g. Chinese).\n"
	"A standard Matlab/Octave char()acter text string is interpreted according to Screen's "
	"current character encoding setting. By default this is the \"system default locale\", as "
	"selected in the language settings of your user account. You can change the encoding "
	"anytime via a call to Screen('Preference', 'TextEncodingLocale', newencoding); "
	"E.g., for UTF-8 multibyte character encoding you'd call Screen('Preference','TextEncodingLocale','UTF-8');\n"
	"If you have a non-ASCII text string and want to make sure that Matlab or Octave doesn't "
	"meddle with your string, convert it into a uint8() datatype before passing to this function.\n"
	"If you want to pass a string which contains unicode characters directly, convert the "
	"text to a double matrix, e.g., mytext = double(myunicodetext); then pass the double "
	"matrix to this function. Screen will interpret all double numbers directly as unicode "
	"code points.\n"
	"Unicode text drawing is supported on all operating systems if you select the default "
	"high quality text renderer. Of course you also have to select a text font which contains "
	"the unicode character sets you want to draw - not all fonts contain all unicode characters.\n"
	"With the optionally selectable fast, low quality renderer, neither anti-aliasing nor Unicode "
	"are supported and text positioning may be less accurate, but it is a good choice if "
	"you are in need for speed over everything else. Select it via the command:\n"
	"Screen('Preference', 'TextRenderer', 0); inserted at the top of your script.\n"
	"The following optional parameters allow to control location and color of the drawn text:\n"
    "\"x\" \"y\" defines the text pen startlocation. Default is the location of the pen from "
	"previous draw text commands, or (0,0) at startup. \"color\" is the CLUT index (scalar or [r "
    "g b] triplet or [r g b a] quadruple) for drawing the text; default produces black.\n"
    "\"backgroundColor\" is the color of the background area behind the text. By default, "
	"text is drawn transparent in front of whatever image content is stored in the window. "
	"You need to set an explicit backgroundColor and possibly enable user defined alpha-blending "
	"with Screen('Preference', 'TextAlphaBlending', 1); and Screen('Blendfunction', ...) to make "
	"use of text background drawing. Appearance of the background + text may be different accross "
	"different operating systems and text renderers, or it may not be supported at all, so this is "
	"not a feature to rely on.\n"
	"\"yPositionIsBaseline\" If specified, will override the global preference setting for text "
	"positioning: It defaults to off. If it is set to 1, then the \"y\" pen start location defines "
	"the base line of drawn text, otherwise it defines the top of the drawn text. Old PTB's had a "
	"behaviour equivalent to setting 1, unfortunately this behaviour wasn't replicated in early "
	"versions of Psychtoolbox-3, so now we stick to the new behaviour by default.\n"
	"\"swapTextDirection\" If specified and set to 1, then the direction of the text is swapped "
	"from the default left-to-right to the swapped right-to-left direction, e.g., to handle scripts "
	"with right-to-left writing order like hebrew.\n"
	"\"newX, newY\" optionally return the final pen location.\n"
	"Btw.: Screen('Preference', ...); provides a couple of interesting text preference "
	"settings that affect text drawing, e.g., setting alpha blending and anti-aliasing modes.\n"
	"Selectable text renderers: The Screen('Preference', 'TextRenderer', Type); command allows "
	"to select among different text rendering engines with different properties:\n"
	"Type 0 is the fast OS specific text renderer: No unicode support, no anti-aliasing, low flexibility "
	"but high speed for fast text drawing. Supported on Windows and Linux as a OpenGL display list renderer.\n"
	"Type 1 is the OS specific high quality renderer: Slower, but supports unicode, anti-aliasing, and "
	"many interesting features. On Windows, this is a GDI based renderer, on OS/X it is Apple's ATSU "
	"text renderer which is also used for Type 0 on OS/X. On Linux it is a renderer based on FTGL.\n"
	"Type 2 is a renderer based on FTGL, the same as type 1 on Linux, also available on OS/X, not supported "
	"on Windows.\n"
	"This function doesn't provide support for text layout. Use the higher level DrawFormattedText() function "
	"if you need basic support for text layout, e.g, centered text output, line wrapping etc.\n";

static char seeAlsoString[] = "TextBounds TextSize TextFont TextStyle TextColor TextBackgroundColor Preference";

// OS/X specific default renderer:
#if PSYCH_SYSTEM == PSYCH_OSX

#define USE_ATSU_TEXT_RENDER	1

//Specify arguments to glTexImage2D when creating a texture to be held in client RAM. The choices are dictated  by our use of Apple's 
//GL_UNPACK_CLIENT_STORAGE_APPLE constant, an argument to glPixelStorei() which specifies the packing format of pixel the data passed to 
//glTexImage2D().  
#define texImage_target			GL_TEXTURE_2D
#define texImage_level			0
#define	texImage_internalFormat	GL_RGBA
#define texImage_sourceFormat	GL_BGRA
#define texImage_sourceType		GL_UNSIGNED_INT_8_8_8_8_REV

//Specify arguments to CGBitmapContextCreate() when creating a CG context which matches the pixel packing of the texture stored in client memory.
//The choice of values is dictated by our use arguments to    
#define cg_RGBA_32_BitsPerPixel		32
#define cg_RGBA_32_BitsPerComponent	8

PsychError	PsychOSDrawUnicodeText(PsychWindowRecordType* winRec, PsychRectType* boundingbox, unsigned int stringLengthChars, double* textUniDoubleString, double* xp, double* yp, unsigned int yPositionIsBaseline, PsychColorType *textColor, PsychColorType *backgroundColor)
{
	char			errmsg[1000];
    CGContextRef	cgContext;
    unsigned int	memoryTotalSizeBytes, memoryRowSizeBytes;
    UInt32			*textureMemory;
    GLuint			myTexture;
    CGColorSpaceRef	cgColorSpace;
    CGRect			quartzRect;
    GLdouble		backgroundColorVector[4];
    UniChar			*textUniString;
    OSStatus		callError;
    ATSUStyle		atsuStyle;
    ATSUTextLayout	textLayout;
    Rect			textBoundsQRect;
    double			textBoundsPRect[4], textBoundsPRectOrigin[4], textureRect[4];
    double			textureWidth, textureHeight, textHeight, textWidth, textureTextFractionY, textureTextFractionXLeft,textureTextFractionXRight, textHeightToBaseline;
    double			quadLeft, quadRight, quadTop, quadBottom;
    GLenum			normalSourceBlendFactor, normalDestinationBlendFactor;
	int				dummy1, dummy2;
	int				ix;
	GLubyte			*rpb;
	psych_bool		bigendian;
	
	// Detect endianity (byte-order) of machine:
    ix = 255;
    rpb = (GLubyte*) &ix;
    bigendian = ( *rpb == 255 ) ? FALSE : TRUE;
    ix = 0; rpb = NULL;
    
    // For layout attributes.  (not the same as run style attributes set by PsychSetATSUTStyleAttributes or line attributes which we do not set.) 	
    ATSUAttributeTag			saTags[] =  {kATSUCGContextTag };
    ByteCount					saSizes[] = {sizeof(CGContextRef)};
    ATSUAttributeValuePtr       saValue[] = {&cgContext};

	// Convert input text string from double-vector encoding to OS/X UniChar encoding:
	textUniString = (UniChar*) PsychMallocTemp(sizeof(UniChar) * stringLengthChars);
	for (dummy1 = 0; dummy1 < stringLengthChars; dummy1++) textUniString[dummy1] = (UniChar) textUniDoubleString[dummy1];
	
	//create the text layout object
    callError=ATSUCreateTextLayout(&textLayout);
    //associate our unicode text string with the text layout object
    callError=ATSUSetTextPointerLocation(textLayout, textUniString, kATSUFromTextBeginning, kATSUToTextEnd, (UniCharCount)stringLengthChars);
    //create an ATSU style object and tie it to the layout object in a style run.
    callError=ATSUCreateStyle(&atsuStyle);
    callError=ATSUClearStyle(atsuStyle);
    PsychSetATSUStyleAttributesFromPsychWindowRecord(atsuStyle, winRec);
    callError=ATSUSetRunStyle(textLayout, atsuStyle, (UniCharArrayOffset)0, (UniCharCount)stringLengthChars);
    /////////////end common to TextBounds and DrawText//////////////////
    
	// Define the meaning of the y position of the specified drawing cursor.
	if (yPositionIsBaseline) {
		// Y position of drawing cursor defines distance between top of text and
		// baseline of text, i.e. the textheight excluding descenders of letters.

		// Need to compute offset via ATSU:
		ATSUTextMeasurement mleft, mright, mtop, mbottom;
        callError=ATSUGetUnjustifiedBounds(textLayout, kATSUFromTextBeginning, kATSUToTextEnd, &mleft, &mright, &mbottom, &mtop);
		if (callError) {
			PsychErrorExitMsg(PsychError_internal, "Failed to compute unjustified text height to baseline in call to ATSUGetUnjustifiedBounds().\n");    
		}

		// Only take height including ascenders into account, not the descenders.
		// MK: Honestly, i have no clue why this is the correct calculation (or if it is
		// the correct calculation), but visually it seems to provide the correct results
		// and i'm not a typographic expert and don't intend to become one...
		textHeightToBaseline = fabs(Fix2X(mbottom));
	}
	else {
		// Y position of drawing cursor defines top of text, therefore no offset (==0) needed:
		textHeightToBaseline = 0;
	}

    // Get the bounds for our text and create a texture of sufficient size to contain it. 
    ATSTrapezoid trapezoid;
    ItemCount oActualNumberOfBounds = 0;
    callError=ATSUGetGlyphBounds(textLayout, 0, 0, kATSUFromTextBeginning, kATSUToTextEnd, kATSUseDeviceOrigins, 0, NULL, &oActualNumberOfBounds);
    if (callError || oActualNumberOfBounds!=1) {
        PsychErrorExitMsg(PsychError_internal, "Failed to compute bounding box in call 1 to ATSUGetGlyphBounds() (nrbounds!=1)\n");    
    }
	
    callError=ATSUGetGlyphBounds(textLayout, 0, 0, kATSUFromTextBeginning, kATSUToTextEnd, kATSUseDeviceOrigins, 1, &trapezoid, &oActualNumberOfBounds);
    if (callError || oActualNumberOfBounds!=1) {
        PsychErrorExitMsg(PsychError_internal, "Failed to retrieve bounding box in call 2 to ATSUGetGlyphBounds() (nrbounds!=1)\n");    
    }
    
    textBoundsPRect[kPsychLeft]=(Fix2X(trapezoid.upperLeft.x) < Fix2X(trapezoid.lowerLeft.x)) ? Fix2X(trapezoid.upperLeft.x) : Fix2X(trapezoid.lowerLeft.x);
    textBoundsPRect[kPsychRight]=(Fix2X(trapezoid.upperRight.x) > Fix2X(trapezoid.lowerRight.x)) ? Fix2X(trapezoid.upperRight.x) : Fix2X(trapezoid.lowerRight.x);
    textBoundsPRect[kPsychTop]=(Fix2X(trapezoid.upperLeft.y) < Fix2X(trapezoid.upperRight.y)) ? Fix2X(trapezoid.upperLeft.y) : Fix2X(trapezoid.upperRight.y);
    textBoundsPRect[kPsychBottom]=(Fix2X(trapezoid.lowerLeft.y) > Fix2X(trapezoid.lowerRight.y)) ? Fix2X(trapezoid.lowerLeft.y) : Fix2X(trapezoid.lowerRight.y);
    
    // printf("Top %lf x Bottom %lf :: ",textBoundsPRect[kPsychTop], textBoundsPRect[kPsychBottom]); 
    PsychNormalizeRect(textBoundsPRect, textBoundsPRectOrigin);

	// Only text boundingbox in absolute coordinates requested?
	if (boundingbox) {
		// Yes. Compute and assign it:
		(*boundingbox)[kPsychLeft]   = textBoundsPRectOrigin[kPsychLeft]   + *xp;
		(*boundingbox)[kPsychRight]  = textBoundsPRectOrigin[kPsychRight]  + *xp;
		(*boundingbox)[kPsychTop]    = textBoundsPRectOrigin[kPsychTop]    + *yp - textHeightToBaseline;
		(*boundingbox)[kPsychBottom] = textBoundsPRectOrigin[kPsychBottom] + *yp - textHeightToBaseline;
		
		// Release resources:
		ATSUDisposeStyle(atsuStyle);
		ATSUDisposeTextLayout(textLayout);

		// Done.
		return(PsychError_none);
	}

    // printf("N: Top %lf x Bottom %lf :: ",textBoundsPRectOrigin[kPsychTop], textBoundsPRectOrigin[kPsychBottom]);
	// Denis found an off-by-one bug in the text width. Don't know where it should come from in our code, but
	// my "solution" is to simply extend the width by one: 
    textWidth=PsychGetWidthFromRect(textBoundsPRectOrigin) + 1.0;
    textHeight=PsychGetHeightFromRect(textBoundsPRectOrigin);
	
	// Clamp maximum size of text bitmap to maximum supported texture size of GPU:
	if (textWidth > winRec->maxTextureSize) textWidth = winRec->maxTextureSize;
	if (textHeight > winRec->maxTextureSize) textHeight = winRec->maxTextureSize;

    // printf("N: Width %lf x Height %lf :: ", textWidth, textHeight); 
    PsychFindEnclosingTextureRect(textBoundsPRectOrigin, textureRect);

    //Allocate memory the size of the texture.  The CG context is the same size.  It could be smaller, because Core Graphics surfaces don't have the power-of-two
    //constraint requirement.   
    textureWidth=PsychGetWidthFromRect(textureRect);
    textureHeight=PsychGetHeightFromRect(textureRect);

	// Reclamp maximum size of text bitmap to maximum supported texture size of GPU:
	if (textureWidth > winRec->maxTextureSize) textureWidth = winRec->maxTextureSize;
	if (textureHeight > winRec->maxTextureSize) textureHeight = winRec->maxTextureSize;
	
    memoryRowSizeBytes=sizeof(UInt32) * textureWidth;
    memoryTotalSizeBytes= memoryRowSizeBytes * textureHeight;
    textureMemory=(UInt32 *)valloc(memoryTotalSizeBytes);
    if(!textureMemory) PsychErrorExitMsg(PsychError_internal, "Failed to allocate surface memory\n");

    // printf("N: TexWidth %lf x TexHeight %lf :: ", textureWidth, textureHeight);
	
	// This zero-fill of memory should not be neccessary, but it is, as a workaround for some bug introduced
	// by Apple into OS/X 10.6.0 -- Apparently fails to initialize memory properly, so pixeltrash gets through...
    memset(textureMemory, 0, memoryTotalSizeBytes);

    // Create the Core Graphics bitmap graphics context.  We can tell CoreGraphics to use the same memory storage format as will our GL texture, and in fact use
    // the idential memory for both.   
    cgColorSpace=CGColorSpaceCreateDeviceRGB();

    // There is another OSX bug here.  the format constant should be ARGB not RBGA to agree with the texture format.           
    cgContext= CGBitmapContextCreate(textureMemory, textureWidth, textureHeight, 8, memoryRowSizeBytes, cgColorSpace, kCGImageAlphaPremultipliedFirst);
    if(!cgContext){
        free((void *)textureMemory);
		printf("PTB-ERROR: In Screen('DrawText'): Failed to allocate CG Bitmap Context for: texWidth=%i, texHeight=%i, memRowSize=%i\n", textureWidth, textureHeight, memoryRowSizeBytes);
		printf("PTB-ERROR: In Screen('DrawText'): xPos=%lf yPos=%lf StringLength=%i\nDecoded Unicode-String:\n", *xp, *yp, stringLengthChars);
		for (ix=0; ix < stringLengthChars; ix++) printf("%i, ", (int) textUniString[ix]);
		printf("\nPTB-ERROR: In Screen('DrawText'): Text corrupt?!?\n");
		
        goto drawtext_skipped;
    }
	
    CGContextSetFillColorSpace (cgContext,cgColorSpace);
        
    // Fill in the text background.  It's stored in the Window record in PsychColor format.  We convert it to an OpenGL color vector then into a quartz vector:
    quartzRect.origin.x=(float)0;
    quartzRect.origin.y=(float)0;
    quartzRect.size.width=(float)textureWidth;
    quartzRect.size.height=(float)textureHeight;
    PsychCoerceColorMode(backgroundColor);
    PsychConvertColorToDoubleVector(backgroundColor, winRec, backgroundColorVector);

	// Override alpha-blending settings if needed:
    if(!PsychPrefStateGet_TextAlphaBlending()) backgroundColorVector[3]=0;
    
    CGContextSetRGBFillColor(cgContext, (float)(backgroundColorVector[0]), (float)(backgroundColorVector[1]), (float)(backgroundColorVector[2]), (float)(backgroundColorVector[3])); 
    CGContextFillRect(cgContext, quartzRect);
    
    // Now draw the text and close up the CoreGraphics shop before we proceed to textures.
    // associate the core graphics context with text layout object holding our unicode string.
    callError=ATSUSetLayoutControls (textLayout, 1, saTags, saSizes, saValue);
    ATSUDrawText(textLayout, kATSUFromTextBeginning, kATSUToTextEnd, Long2Fix((long)0), Long2Fix((long) textBoundsPRect[kPsychBottom]));
    CGContextFlush(cgContext);

	// Free ATSUI stuff that is no longer needed:
    ATSUDisposeStyle(atsuStyle);
	ATSUDisposeTextLayout(textLayout);

    //Remove references from Core Graphics to the texture memory.  CG and OpenGL can share concurrently, but we don't won't need this anymore.
    CGColorSpaceRelease (cgColorSpace);
    CGContextRelease(cgContext);	
    
    // From here on: Convert the CG graphics bitmap into a GL texture.  

    // Enable this windowRecords framebuffer as current drawingtarget:
    PsychSetDrawingTarget(winRec);

	// Save all state:
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	// Disable draw shader:
	PsychSetShader(winRec, 0);

    if(!PsychPrefStateGet_TextAlphaBlending()){
        PsychGetAlphaBlendingFactorsFromWindow(winRec, &normalSourceBlendFactor, &normalDestinationBlendFactor);
        PsychStoreAlphaBlendingFactorsForWindow(winRec, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    PsychUpdateAlphaBlendingFactorLazily(winRec);
    
    // Explicitely disable Apple's Client storage extensions. For now they are not really useful to us.
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
    
    glDisable(GL_TEXTURE_RECTANGLE_EXT);
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &myTexture);				//create an index "name" for our texture
    glBindTexture(GL_TEXTURE_2D, myTexture);	//instantiate a texture of type associated with the index and set it to be the target for subsequent gl texture operators.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);		//tell gl how to unpack from our memory when creating a surface, namely don't really unpack it but use it for texture storage.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);	//specify interpolation scaling rule for copying from texture.  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);  //specify interpolation scaling rule from copying from texture.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    PsychTestForGLErrors();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  (GLsizei)textureWidth, (GLsizei)textureHeight, 0, GL_BGRA, (bigendian) ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_INT_8_8_8_8, textureMemory);
    free((void *)textureMemory);	// Free the texture memory: OpenGL has its own copy now in internal buffers.
    textureMemory = NULL;
    
    PsychTestForGLErrors();
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    
    //The texture holding the text is >=  the bounding rect for the text because of the requirement for textures that they have sides 2^(integer)x.  
    //Therefore we texture only from the region of the texture which contains the text, not the entire texture.  Therefore the rect which we texture is the dimensions
    //of the text, not the dimensions of the texture.    
    textureTextFractionXLeft = 0 / textureWidth;
    textureTextFractionXRight = textWidth / textureWidth;
    textureTextFractionY = 1.0 - textHeight / textureHeight;

    // Final screen position of the textured text-quad:
    quadLeft  = *xp;
    quadRight = *xp + textWidth;
	// quadTop needs to be adjusted by textHeightToBaseline, see above:
    quadTop = *yp - textHeightToBaseline;
    quadBottom = quadTop + textHeight;
    
    // Submit quad to pipeline:
    glBegin(GL_QUADS);
    glTexCoord2d(textureTextFractionXLeft, textureTextFractionY);		glVertex2d(quadLeft, quadTop);
    glTexCoord2d(textureTextFractionXRight, textureTextFractionY);		glVertex2d(quadRight, quadTop);
    glTexCoord2d(textureTextFractionXRight, 1.0);				glVertex2d(quadRight, quadBottom);
    glTexCoord2d(textureTextFractionXLeft, 1.0);				glVertex2d(quadLeft, quadBottom);
    glEnd();
    
    // Done with this texture:
    glDisable(GL_TEXTURE_2D);
    
    // Debug: Visualize bounding box of text on-screen:
    if (false) { 
        glColor3f(1,1,1);
        glBegin(GL_LINE_LOOP);
        glVertex2d(quadLeft, quadTop);
        glVertex2d(quadRight, quadTop);
        glVertex2d(quadRight, quadBottom);
        glVertex2d(quadLeft, quadBottom);
        glEnd();
    }
    PsychTestForGLErrors();
    
    if(!PsychPrefStateGet_TextAlphaBlending()) PsychStoreAlphaBlendingFactorsForWindow(winRec, normalSourceBlendFactor, normalDestinationBlendFactor);

    // Remove references from gl to the texture memory  & free gl's associated resources
    glDeleteTextures(1, &myTexture);	 
    
	glPopAttrib();
	
    // Update drawing cursor: Place cursor so that text could
    // be appended right-hand of the drawn text.
    *xp = quadRight;

	// We jump directly to this position in the code if the textstring is empty --> No op.
drawtext_skipped:        
    return(PsychError_none);
}

#else

// Implementations for Windows and Linux/X11:

#if PSYCH_SYSTEM == PSYCH_WINDOWS

// Protototype of internal GDI text renderer to be called later in this file:
PsychError PsychOSDrawUnicodeTextGDI(PsychWindowRecordType* winRec, PsychRectType* boundingbox, unsigned int stringLengthChars, double* textUniDoubleString, double* xp, double* yp, unsigned int yPositionIsBaseline, PsychColorType *textColor, PsychColorType *backgroundColor);

// Microsoft-Windows implementation of DrawText...
// The code below will need to be restructured and moved to the proper
// places in PTB's source tree when things have stabilized a bit...

/* PsychOSReBuildFont
 *
 * (Re)Build a font for the specified winRec, based on OpenGL display lists.
 *
 * This routine examines the font settings for winRec and builds proper
 * OpenGL display lists that represent a font as close as possible to the
 * requested font. These routines are specific to Microsoft Windows, so they
 * need to be reimplemented for other OS'es...
 */
psych_bool PsychOSRebuildFont(PsychWindowRecordType *winRec)
{
  GLYPHMETRICSFLOAT	gmf[256];	// Address Buffer For Font Storage
  HFONT	font, oldfont;			// Windows Font ID
  GLuint base;
  int i;

  // Does font need to be rebuild?
  if (!winRec->textAttributes.needsRebuild) {
    // No rebuild needed. We don't have anything to do.
    return(TRUE);
  }

  // Rebuild needed. Do we have already a display list?
  if (winRec->textAttributes.DisplayList > 0) {
    // Yep. Destroy it...
    glDeleteLists(winRec->textAttributes.DisplayList, 256);
    winRec->textAttributes.DisplayList=0;
  }

  // Create Windows font object with requested properties:
  font = NULL;
  font = CreateFont(((int) (-MulDiv(winRec->textAttributes.textSize, GetDeviceCaps(winRec->targetSpecific.deviceContext, LOGPIXELSY), 72))),				// Height Of Font, aka textSize
			0,							                // Width Of Font: 0=Match to height
			0,							                // Angle Of Escapement
			0,							                // Orientation Angle
			((winRec->textAttributes.textStyle & 1) ? FW_BOLD : FW_NORMAL),		// Font Weight
			((winRec->textAttributes.textStyle & 2) ? TRUE : FALSE),		// Italic
			((winRec->textAttributes.textStyle & 4) ? TRUE : FALSE),		// Underline
			FALSE,		                // Strikeout: Set it to false until we know what it actually means...
			ANSI_CHARSET,			// Character Set Identifier: Would need to be set different for "WingDings" fonts...
			OUT_TT_PRECIS,			// Output Precision:   We try to get TrueType fonts if possible, but allow fallback to low-quality...
			CLIP_DEFAULT_PRECIS,		// Clipping Precision: Use system default.
			ANTIALIASED_QUALITY,		// Output Quality:     We want antialiased smooth looking fonts.
			FF_DONTCARE|DEFAULT_PITCH,	// Family And Pitch:   Use system default.
			(char*) winRec->textAttributes.textFontName);		// Font Name as requested by user.
  
  // Child-protection:
  if (font==NULL) {
    // Something went wrong...
    PsychErrorExitMsg(PsychError_user, "Couldn't select the requested font with the requested font settings from Windows-OS! ");
    return(FALSE);
  }

  // Select the font we created: Retain old font handle for restore below...
  oldfont=SelectObject(winRec->targetSpecific.deviceContext, font);		// Selects The Font We Created

  // Activate OpenGL context:
  PsychSetGLContext(winRec);

  // Generate 256 display lists, one for each ASCII character:
  base = glGenLists(256);
  
  // Build the display lists from the font: We want an outline font instead of a bitmapped one.
  // Characters of outline fonts are build as real OpenGL 3D objects (meshes of connected polygons)
  // with normals, texture coordinates and so on, so they can be rendered and transformed in 3D, including
  // proper texturing and lighting...
  wglUseFontOutlines(winRec->targetSpecific.deviceContext,			// Select The Current DC
		     0,								// Starting Character is ASCII char zero.
		     256,							// Number Of Display Lists To Build: 256 for all 256 chars.
		     base,							// Starting Display List handle.
		     0.0f,							// Deviation From The True Outlines: Smaller value=Smoother, but more geometry.
		     0.2f,							// Font Thickness In The Z Direction for 3D rendering.
		     ((winRec->textAttributes.textStyle & 8) ? WGL_FONT_LINES : WGL_FONT_POLYGONS),	    // Type of rendering: Filled polygons or just outlines?
		     gmf);							// Address Of Buffer To receive font metrics data.

  // Assign new display list:
  winRec->textAttributes.DisplayList = base;
  // Clear the rebuild flag:
  winRec->textAttributes.needsRebuild = FALSE;

  // Copy glyph geometry info into winRec:
  for(i=0; i<256; i++) {
    winRec->textAttributes.glyphWidth[i]=(float) gmf[i].gmfCellIncX;
    winRec->textAttributes.glyphHeight[i]=(float) gmf[i].gmfCellIncY;
  }

  // Clean up after font creation:
  SelectObject(winRec->targetSpecific.deviceContext, oldfont);		        // Restores current font selection to previous setting.
  DeleteObject(font); // Delete the now orphaned font object.

  // Our new font is ready to rock!
  return(TRUE);
}

#endif

#if PSYCH_SYSTEM == PSYCH_LINUX

// Linux/X11 implementation of PsychOSRebuildFont():

// Include of tolower() function:
#include <ctype.h>

psych_bool PsychOSRebuildFont(PsychWindowRecordType *winRec)
{
  char fontname[512];
  char** fontnames=NULL;
  Font font;
  XFontStruct* fontstruct=NULL;
  GLuint base;
  int i, actual_count_return;

  // Does font need to be rebuild?
  if (!winRec->textAttributes.needsRebuild) {
    // No rebuild needed. We don't have anything to do.
    return(TRUE);
  }

  // Rebuild needed. Do we have already a display list?
  if (winRec->textAttributes.DisplayList > 0) {
    // Yep. Destroy it...
    glDeleteLists(winRec->textAttributes.DisplayList, 256);
    winRec->textAttributes.DisplayList=0;
  }

  // Create X11 font object with requested properties:
  if (winRec->textAttributes.textFontName[0] == '-') {
    // Fontname supplied in X11 font name format. Just take it as is,
    // the user seems to know how to handle X11 fonts...
    snprintf(fontname, sizeof(fontname)-1, "*%s*", winRec->textAttributes.textFontName); 
  }
  else {
    // Standard Psychtoolbox font name spec: Use all the text settings that we have and
    // try to synthesize a X11 font spec string.
    snprintf(fontname, sizeof(fontname)-1, "-*-%s-%s-%s-*--%i-*-*-*", winRec->textAttributes.textFontName, ((winRec->textAttributes.textStyle & 1) ? "bold" : "regular"),
	     ((winRec->textAttributes.textStyle & 2) ? "i" : "r"), winRec->textAttributes.textSize); 
  }

  fontname[sizeof(fontname)-1]=0;
  // Convert fontname to lower-case characters:
  for(i=0; i<strlen(fontname); i++) fontname[i]=tolower(fontname[i]);

  // Try to load font:
  fontstruct = XLoadQueryFont(winRec->targetSpecific.deviceContext, fontname); 

  // Child-protection against invalid fontNames or unavailable/unknown fonts:
  if (fontstruct == NULL) {
    // Something went wrong...
    printf("Failed to load X11 font with name %s.\n\n", winRec->textAttributes.textFontName);
    fontnames = XListFonts(winRec->targetSpecific.deviceContext, "*", 1000, &actual_count_return);
    if (fontnames) {
      printf("Available X11 fonts are:\n\n");
      for (i=0; i<actual_count_return; i++) printf("%s\n", (char*) fontnames[i]);
      printf("\n\n");
      XFreeFontNames(fontnames);
      fontnames=NULL;
    }

    printf("Failed to load X11 font with name %s.\n", fontname);
    printf("Try a Screen('TextFont') name according to one of the listed available fonts above.\n\n");
    PsychErrorExitMsg(PsychError_user, "Couldn't select the requested font with the requested font settings from X11 system!");
    return(FALSE);
  }

  // Get font handle from struct:
  font = fontstruct->fid;

  // Activate OpenGL context:
  PsychSetGLContext(winRec);

  // Generate 256 display lists, one for each ASCII character:
  base = glGenLists(256);

  // Build the display lists from the font:
  glXUseXFont(font,
	      0,                   // Starting Character is ASCII char zero.
              256,                 // Number Of Display Lists To Build: 256 for all 256 chars.
              base                 // Starting Display List handle.
	      );
  
  // Assign new display list:
  winRec->textAttributes.DisplayList = base;

  // Clear the rebuild flag:
  winRec->textAttributes.needsRebuild = FALSE;

  // Copy glyph geometry info into winRec:
  for(i=0; i<256; i++) {
    fontname[0]=(char) i;
    fontname[1]=0;
    winRec->textAttributes.glyphWidth[i]=(float) XTextWidth(fontstruct, fontname, 1);
    winRec->textAttributes.glyphHeight[i]=(float) winRec->textAttributes.textSize;
  }

  // Release font and associated font info:
  XFreeFontInfo(NULL, fontstruct, 1);
  fontstruct=NULL;
  XUnloadFont(winRec->targetSpecific.deviceContext, font);

  // Our new font is ready to rock!
  return(TRUE);
}
#endif


// The DrawText implementation itself is identical on Windows and Linux for the simple displaylist-based renderers:
PsychError	PsychOSDrawUnicodeText(PsychWindowRecordType* winRec, PsychRectType* boundingbox, unsigned int stringLengthChars, double* textUniDoubleString, double* xp, double* yp, unsigned int yPositionIsBaseline, PsychColorType *textColor, PsychColorType *backgroundColor)
{
    char			*textString;
    unsigned int	i;
    float			accumWidth, maxHeight, textHeightToBaseline, scalef; 

    #if PSYCH_SYSTEM == PSYCH_WINDOWS
		 // Use GDI based text renderer on Windows, instead of display list based one?
		 if (PsychPrefStateGet_TextRenderer() == 1) {
			// Call the GDI based renderer instead:
			return(PsychOSDrawUnicodeTextGDI(winRec, boundingbox, stringLengthChars, textUniDoubleString, xp, yp, yPositionIsBaseline, textColor, backgroundColor));
	 	 }
	#endif

	// Malloc charstring and convert unicode string to char string:
	textString = (char*) PsychMallocTemp(stringLengthChars * sizeof(char));
    for (i = 0; i < stringLengthChars; i++) textString[i] = (char) textUniDoubleString[i];

	// Boundingbox computation or real text drawing?
	if (boundingbox) {
		// Enable this windowRecords OpenGL context:
		PsychSetGLContext(winRec);
	}
	else {
		// Enable this windowRecords framebuffer as current drawingtarget:
		PsychSetDrawingTarget(winRec);
	}
	
    // Does the font (better, its display list) need to be build or rebuild, because
    // font name, size or settings have changed?
    // This routine will check it and perform all necessary ops if so...
    PsychOSRebuildFont(winRec);

    // Compute text-bounds as x and y increments:
    accumWidth=0;
    maxHeight=0;
    for (i = 0; i < stringLengthChars; i++) {
      accumWidth += winRec->textAttributes.glyphWidth[textString[i]];
      maxHeight   = (winRec->textAttributes.glyphHeight[textString[i]] > maxHeight) ? winRec->textAttributes.glyphHeight[textString[i]] : maxHeight;
    }

    accumWidth *= (PSYCH_SYSTEM == PSYCH_WINDOWS) ? winRec->textAttributes.textSize : 1.0;
    maxHeight  *= (PSYCH_SYSTEM == PSYCH_WINDOWS) ? winRec->textAttributes.textSize : 1.0;

	if (yPositionIsBaseline) {
		// Y position of drawing cursor defines distance between top of text and
		// baseline of text, i.e. the textheight excluding descenders of letters:
		// FIXME: This is most likely plain wrong!!!
		textHeightToBaseline = maxHeight;
	}
	else {
		// Y position of drawing cursor defines top of text, therefore no offset (==0) needed:
		textHeightToBaseline = 0;
	}

	// Boundingbox computation or real text drawing?
	if (boundingbox) {
		// Only computation of bounding box requested, no real text drawing:

        // Top-Left bounds of text are current (x,y) position of text drawing cursor:
		(*boundingbox)[kPsychLeft]  = *xp;
		(*boundingbox)[kPsychTop]   = *yp;
		(*boundingbox)[kPsychRight] = *xp + accumWidth;
		// MK: This should work according to spec, but f%$!*g Windows only returns zero values
		// for glyphHeight, so maxHeight is always zero :(
		// (*boundingbox)[kPsychBottom] = *yp + maxHeight;
		//
		// As fallback, we use this: It gives correct Bottom-Bound for character strings with characters that
		// don't contain descenders. The extra height of characters with descenders is not taken into account.
		(*boundingbox)[kPsychBottom] = *yp + winRec->textAttributes.textSize;
		
		// Done.
		return(PsychError_none);
	}

	// Set default draw shader on Windows, but disable on Linux, as glBitmapped rendering doesn't work with it:
	PsychSetShader(winRec, (PSYCH_SYSTEM == PSYCH_LINUX) ? 0 : -1);

	// Set proper alpha-blending mode:
	PsychUpdateAlphaBlendingFactorLazily(winRec);

	// Set proper color:
    PsychSetGLColor(textColor, winRec);

    // Backup modelview matrix:
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    #if PSYCH_SYSTEM == PSYCH_WINDOWS
		// Position our "cursor": These are 3D fonts where the glyphs are represented by 3D geometry.
		glTranslatef(*xp, *yp - textHeightToBaseline + winRec->textAttributes.textSize, -0.5f);

		// Scale to final size:
		scalef = MulDiv(winRec->textAttributes.textSize, GetDeviceCaps(winRec->targetSpecific.deviceContext, LOGPIXELSY), 72);
		glScalef(scalef, -1 * scalef, 1);
    #endif

    #if PSYCH_SYSTEM == PSYCH_LINUX
		// Position our "cursor": The X11 implementation uses glBitmap()'ed fonts, so we need to position
		// the rasterposition cursor. We need the glColor4dv() here to handle a special case when using
		// HDR framebuffers and HDR draw shaders -- this is not compatible with Linux glBitmapped() rendering...
		glColor4dv(winRec->currentColor);
		glRasterPos2f(*xp, *yp  - textHeightToBaseline + winRec->textAttributes.textSize);
		glPixelZoom(1,1);
    #endif

    // Backup display list state and state of glFrontFace(): The display lists on M$-Windows
    // contain glFrontFace() commands to change front-face order, so we need to save and
    // restore it.
    glPushAttrib(GL_LIST_BIT | GL_POLYGON_BIT);

    // Sets The Base Character to the start of our font display list:
    glListBase(winRec->textAttributes.DisplayList);

    // Render it...
    glCallLists(stringLengthChars, GL_UNSIGNED_BYTE, textString);

    // Restore state:
    glPopAttrib();
    glPopMatrix();

    // Mark end of drawing op. This is needed for single buffered drawing:
    PsychFlushGL(winRec);

    // Update drawing cursor: Place cursor so that text could be appended right-hand of the drawn text.
    *xp = *xp + accumWidth;

	// Done.
    return(PsychError_none);
}

#if PSYCH_SYSTEM == PSYCH_WINDOWS

// GDI based text-renderer for MS-Windows:
//
// It's sloooow. However it provides accurate text positioning, Unicode rendering,
// anti-aliasing, proper text size and a higher quality text output in general.
//
// It uses GDI text renderer to render text to a memory device context,
// backed by a DIB device independent memory bitmap. Then it converts the
// DIB to an OpenGL compatible RGBA format and draws it via OpenGL,
// currently via glDrawPixels, in the future maybe via texture mapping if
// that should be faster.
//
// Reasons for slowness: GDI is slow and CPU only -- no GPU acceleration,
// GDI->OpenGL data format conversion (and our trick to get an anti-aliased
// alpha-channel) is slow and compute intense, data upload and blit in GL
// is slow due to hostmemory -> VRAM copy.

// The following variables must be released at Screen flush time the latest.
// The exit routine PsychCleanupTextRenderer() does this when invoked
// from the ScreenCloseAllWindows() function, as part of a Screen flush,
// error abort, or Screen('CloseAll').

// The current (last used) font for GDI text drawing:
static HFONT				font=NULL;		// Handle to current font.

// These static variables hold the memory bitmap buffers (device contexts)
// for GDI based text drawing. We keep them accross calls to DrawText, and
// only allocate them on first invocation, or reallocate them when the size
// of the target window has changed.
static HDC					dc = NULL;		// Handle to current memory device context.
static BYTE*				pBits = NULL;	// Pointer to dc's DIB bitmap memory.
static HBITMAP				hbmBuffer;		// DIB.
static HBITMAP				defaultDIB;
static int					oldWidth=-1;	// Size of last target window for drawtext.
static int					oldHeight=-1;	// dto.
static PsychWindowRecordType* oldWin = NULL; // Last window to which text was drawn to.

void CleanupDrawtextGDI(void)
{
	if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: In CleanupDrawtextGDI: Releasing GDI ressources for DrawTextGDI.\n");

	if (font) {
		if (!DeleteObject(font)) printf("PTB-WARNING: In CleanupDrawtextGDI: Failed to release font! Expect memory leaks!!!\n");
	}
	font = NULL;
	
	if (dc) {
		// Unselect hbmBuffer from dc by reselecting default DIB:
		SelectObject(dc, defaultDIB);

		// Release now detached hbmBuffer:
		if (!DeleteObject((HGDIOBJ) hbmBuffer)) printf("PTB-WARNING: In CleanupDrawtextGDI: Failed to release DIB buffer! Expect memory leaks!!!\n");

		// Delete device context:
		if (!DeleteDC(dc)) printf("PTB-WARNING: In CleanupDrawtextGDI: Failed to release device context DC! Expect memory leaks!!!\n");

		hbmBuffer = NULL;
		pBits = NULL;
		dc = NULL;
	}
	
	oldWidth = -1;
	oldHeight = -1;
	
	oldWin = NULL;
	
	return;
}

PsychError	PsychOSDrawUnicodeTextGDI(PsychWindowRecordType* winRec, PsychRectType* boundingbox, unsigned int stringLengthChars, double* textUniDoubleString, double* xp, double* yp, unsigned int yPositionIsBaseline, PsychColorType *textColor, PsychColorType *backgroundColor)
{
    PsychRectType				boundingRect;
	WCHAR*						textUniString;
    int							i;
	GLdouble					incolors[4];
	unsigned char				bincolors[4];
    GLenum						normalSourceBlendFactor, normalDestinationBlendFactor;
	POINT						xy;
    int							BITMAPINFOHEADER_SIZE = sizeof(BITMAPINFOHEADER) ;
	static BITMAPINFOHEADER		abBitmapInfo;
    BITMAPINFOHEADER*			pBMIH = (BITMAPINFOHEADER*) &abBitmapInfo;
	RECT						trect, brect;
	HFONT						defaultFont;
	unsigned char				colorkeyvalue;
	unsigned char*				scanptr;
	int							skiplines, renderheight;	
	DWORD						outputQuality;

	// Convert input double unicode string into WCHAR unicode string for Windows renderer:
	textUniString = (WCHAR*) PsychMallocTemp(sizeof(WCHAR) * stringLengthChars);
	for (i = 0; i < stringLengthChars; i++) textUniString[i] = (WCHAR) textUniDoubleString[i];

	// 'DrawText' mode?
	if (boundingbox == NULL) {
		// DRAWTEXT mode:

		// Enable this windowRecords framebuffer as current drawingtarget:
		PsychSetDrawingTarget(winRec);

		// Set OpenGL drawing color:
		PsychSetGLColor(textColor, winRec);
	}
		
	// Reallocate device context and bitmap if needed:
	if ((dc!=NULL) && (oldWidth != PsychGetWidthFromRect(winRec->rect) || oldHeight!=PsychGetHeightFromRect(winRec->rect))) {
		// Target windows size doesn't match size of our backingstore: Reallocate...
		if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: In DrawTextGDI: Reallocating backing DC due to change in target window size: %i x %i pixels. \n", (int) PsychGetWidthFromRect(winRec->rect), (int) PsychGetHeightFromRect(winRec->rect));

		// Unselect hbmBuffer from dc by reselecting default DIB:
		SelectObject(dc, defaultDIB);

		// Release now detached hbmBuffer:
		if (!DeleteObject((HGDIOBJ) hbmBuffer)) printf("PTB-WARNING: In DrawTextGDI: Failed to release DIB buffer! Expect memory leaks!!!\n");

		// Delete device context:
		if (!DeleteDC(dc)) printf("PTB-WARNING: In DrawTextGDI: Failed to release device context DC! Expect memory leaks!!!\n");

		hbmBuffer = NULL;
		dc = NULL;		
	}
	
	// (Re-)allocation of memory device context and DIB bitmap needed?
	if (dc==NULL) {
		oldWidth=(int) PsychGetWidthFromRect(winRec->rect);
		oldHeight=(int) PsychGetHeightFromRect(winRec->rect);
		
		// Fill in the header info.
		memset(pBMIH, 0, BITMAPINFOHEADER_SIZE);
		pBMIH->biSize         = sizeof(BITMAPINFOHEADER);
		pBMIH->biWidth        = oldWidth;
		pBMIH->biHeight       = oldHeight;
		pBMIH->biPlanes       = 1;
		pBMIH->biBitCount     = 32; 
		pBMIH->biCompression  = BI_RGB; 
		
		//
		// Create the new 32-bpp DIB section.
		//
		dc = CreateCompatibleDC(NULL);
		hbmBuffer = CreateDIBSection( 		  dc,
											  (BITMAPINFO*) pBMIH,
											  DIB_RGB_COLORS,
											  (VOID **) &pBits,
											  NULL,
											  0);

		// Select DIB into DC. Store reference to default DIB:
		defaultDIB = (HBITMAP) SelectObject(dc, hbmBuffer);
	}
	
    // Does the font need to be build or rebuild, because
    // font name, size or settings have changed? Or is the current window
	// winRec not identical to the last target window oldWin? In that case,
	// we'll need to reassign the font as well, as fonts are not cached
	// on a per windowRecord basis.
	//
    // This routine will check it and perform all necessary ops if so...
	if ((winRec->textAttributes.needsRebuild) || (oldWin != winRec)) {
		// Need to realloc font:
		if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: In DrawTextGDI: Rebuilding font due to window switch or rebuild request: needit = %i , oldwin = %p vs. newwin = %p \n", winRec->textAttributes.needsRebuild, oldWin, winRec);
		
		// Delete the old font object, if any:
		if (font) {
			if (!DeleteObject(font)) printf("PTB-WARNING: In DrawTextGDI: Failed to release font! Expect memory leaks!!!\n");
		}
		font = NULL; 

		switch(PsychPrefStateGet_TextAntiAliasing()) {
			case 0:		// No anti-aliasing:
				outputQuality = NONANTIALIASED_QUALITY;
				break;
			case 1:		// Anti-aliased rendering:
				outputQuality = ANTIALIASED_QUALITY;
				break;
			case 2:		// WindowsXP and later only: Cleartype anti-aliasing.
				outputQuality = 5; // This is #define CLEARTYPE_QUALITY ...
				break;
			default:	// Default to anti-aliasing:
				outputQuality = ANTIALIASED_QUALITY;
		}
		
		// Create new font object, according to new/changed specs:
		font = CreateFont(	((int) (-MulDiv(winRec->textAttributes.textSize, GetDeviceCaps(dc, LOGPIXELSY), 72))),	// Height Of Font, aka textSize
							0,							                // Width Of Font: 0=Match to height
							0,							                // Angle Of Escapement
							0,							                // Orientation Angle
							((winRec->textAttributes.textStyle & 1) ? FW_BOLD : FW_NORMAL),		// Font Weight
							((winRec->textAttributes.textStyle & 2) ? TRUE : FALSE),		// Italic
							((winRec->textAttributes.textStyle & 4) ? TRUE : FALSE),		// Underline
							FALSE,		                // Strikeout: Set it to false until we know what it actually means...
							ANSI_CHARSET,			// Character Set Identifier: Would need to be set different for "WingDings" fonts...
							OUT_TT_PRECIS,			// Output Precision:   We try to get TrueType fonts if possible, but allow fallback to low-quality...
							CLIP_DEFAULT_PRECIS,		// Clipping Precision: Use system default.
							outputQuality,		// Output Quality wrt. Anti-Aliasing.
							FF_DONTCARE|DEFAULT_PITCH,	// Family And Pitch:   Use system default.
							(char*) winRec->textAttributes.textFontName);		// Font Name as requested by user.
		
		// Child-protection:
		if (font==NULL) {
			// Something went wrong...
			PsychErrorExitMsg(PsychError_user, "Couldn't select the requested font with the requested font settings from Windows-OS! ");
		}
		
		// Clear rebuild flag:
		winRec->textAttributes.needsRebuild = FALSE;
	}
	
	// Update last target window:
	oldWin = winRec;
	
	// Select the font we created:
	defaultFont = (HFONT) SelectObject(dc, font);

	if (yPositionIsBaseline) {
		// Y position of drawing cursor defines distance between top of text and
		// baseline of text, i.e. the textheight excluding descenders of letters:
		
		// Set text alignment mode to obey and update the drawing cursor position, with the
		// y position being the text baseline:
	 	SetTextAlign(dc, TA_UPDATECP | TA_LEFT | TA_BASELINE);
	}
	else {
		// Y position of drawing cursor defines top of text:
	 	// Set text alignment mode to obey and update the drawing cursor position, with the
		// y position being the top of the text bounding box:
	 	SetTextAlign(dc, TA_UPDATECP | TA_LEFT | TA_TOP);
	}
	
	// Define targetrectangle/cliprectangle for all drawing: It is simply the full
	// target window area:
	trect.left = 0;
	trect.right = oldWidth-1;
	trect.top = 0;
	trect.bottom = oldHeight-1;
	
	// Convert PTB color into text RGBA color and set it as text color:
	PsychConvertColorToDoubleVector(textColor, winRec, incolors);
	
	// Text drawing shall be transparent where no text pixels are drawn:
	SetBkMode(dc, TRANSPARENT);
	
	// Set text color to full white:
	SetTextColor(dc, RGB(255, 255, 255));
	
	// Set drawing cursor to requested position:
	MoveToEx(dc, (int) *xp, (int) *yp, NULL);

	brect = trect;
	// printf("PRE: ltrb %d %d %d %d\n", brect.left, brect.top, brect.right, brect.bottom);

	// Pseudo-Draw the textString: Don't rasterize, just find bounding box.
	DrawTextW(dc, textUniString, stringLengthChars, &brect, DT_CALCRECT);
	MoveToEx(dc, (int) *xp, (int) *yp, NULL);

	// renderheight is the total height of the rendered textbox, not taking clipping into account.
	// Its the number of pixelrows to process...
	renderheight = (int) brect.bottom - (int) brect.top;

	// Calculate skiplines - the number of pixelrows to skip from start of the DIB/from
	// bottom of targetwindow. Need to take into account, what the y position actually means:
	if (yPositionIsBaseline) {
		// y-Position is the baseline of text: Take height of "descender" area into account:
		skiplines = oldHeight - ((renderheight - winRec->textAttributes.textSize) + (int) *yp);
	}
	else {
		// y-Position is top of texts bounding box:
		skiplines = oldHeight - (renderheight + (int) *yp);
	}

	// Calculate and store bounding rectangle:
	boundingRect[kPsychTop]    = oldHeight - 1 - skiplines - renderheight;
	boundingRect[kPsychBottom] = oldHeight - 1 - skiplines;
	boundingRect[kPsychLeft]   = *xp;
	boundingRect[kPsychRight]  = *xp + (double) ((int) brect.right - (int) brect.left);

	// Is this a 'Textbounds' op?
	if (boundingbox) {
		// "Textbounds" op, no real text drawing. Assign final bounding box, then return:
		PsychCopyRect(boundingbox, boundingRect);
		
		// Restore to default font after text drawing:
		SelectObject(dc, defaultFont);

		// Done, return:
		return(PsychError_none);
	}

	// Bounds checking: Need to take text into account that is partially or fully outside
	// the windows drawing area:
	if (skiplines < 0) {
		// Lower bound of text is below lower border of window.
		// Reduce size of processing area by the difference (we add a negative value == subtract):
		renderheight = renderheight + skiplines;
		
		// Start at bottom of screen and DIB with processing:
		skiplines = 0;
	}
	
	if ((skiplines + renderheight) > (oldHeight - 1)) {
		// Upper bound of text is above upper border of window.
		// Reduce size of processing area by the difference:
		renderheight = renderheight - ((skiplines + renderheight) - (oldHeight - 1));
	}

	// Negative or zero renderheight? In that case we would be done, because the area of text
	// to really draw would be empty or less than empty!
	if (renderheight <= 0) goto drawtext_noop;
	
	// Ok, bounds checking left us with something to process and draw - Do it:
	
	// "Erase" DIB with black background color:
	scanptr = (unsigned char*) pBits + skiplines * oldWidth * 4;
	memset((void*) scanptr, 0, oldWidth * renderheight * 4);
	
	// Really draw the textString: Rasterize!
	DrawTextW(dc, textUniString, stringLengthChars, &trect, DT_NOCLIP);

	// Sync the GDI so we have a final valid bitmap after this call:
	GdiFlush();
	
	// Loop through the bitmap: Set the unused MSB of each 32 bit DWORD to a
	// meaningful alpha-value for OpenGL.
	bincolors[0] = (unsigned int)(incolors[0] * 255);
    bincolors[1] = (unsigned int)(incolors[1] * 255);
    bincolors[2] = (unsigned int)(incolors[2] * 255);
    bincolors[3] = (unsigned int)(incolors[3] * 255);

	scanptr = (unsigned char*) pBits + skiplines * oldWidth * 4;
	for (i=0; i<oldWidth * renderheight; i++) {
		*(scanptr++) = bincolors[0];	 // Copy blue text color to blue byte.
		*(scanptr++) = bincolors[1];	 // Copy green text color to green byte.
		// Copy red byte to alpha-channel (its our anti-aliasing alpha-value), but
		// multiply with user spec'd alpha. This multiply-shift is a fast trick to
		// get normalization of the 16 bit multiply:
		colorkeyvalue = (unsigned char)((((unsigned int) *scanptr) * bincolors[3]) >> 8);
		*(scanptr++) = bincolors[2];	 // Copy red text color to red byte.
		*(scanptr++) = colorkeyvalue;	 // Copy final alpha value to alpha byte.
	}
	
	// Save all GL state:
    glPushAttrib(GL_ALL_ATTRIB_BITS);
	
	// Setup alpha-blending for anti-aliasing, unless user script requests us to obey
	// the global blending settings set via Screen('Blendfunction') - which may be
	// suboptimal for anti-aliased text drawing:
    if(!PsychPrefStateGet_TextAlphaBlending()) {
        PsychGetAlphaBlendingFactorsFromWindow(winRec, &normalSourceBlendFactor, &normalDestinationBlendFactor);
        PsychStoreAlphaBlendingFactorsForWindow(winRec, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    PsychUpdateAlphaBlendingFactorLazily(winRec);
	
	// Enable alpha-test against an alpha-value greater zero during blit. This
	// This way, non-text pixess (with alpha equal to zero) are discarded. 
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);

	// To conform to the OS/X behaviour, we only draw a background if user-defined alpha blending
	// is enabled:
    if(PsychPrefStateGet_TextAlphaBlending()) {
		// Draw a background color quad:
		
		// Set GL drawing color:
		PsychSetGLColor(backgroundColor, winRec);

		// Set default draw shader:
		PsychSetShader(winRec, -1);

		// Draw background rect:
		PsychGLRect(boundingRect);
	}

    // Setup unpack mode and position for blitting of the bitmap to screen:
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// MK: Subracting one should be correct, but isn't (visually). Maybe a
	// a side-effect of gfx-rasterizer inaccuracy or off-by-one error in our
	// PsychSetupView() code?!?
	// glRasterPos2i(0,(int) oldHeight - 1 - skiplines);
	glRasterPos2i(0,(int) oldHeight - 0 - skiplines);

	// Blit it to screen: The GL_BGRA swizzles RGBA <-> BGRA properly:
	scanptr = (unsigned char*) pBits + skiplines * oldWidth * 4;

	// Disable draw shader:
	PsychSetShader(winRec, 0);

    glPixelZoom(1,1);
	glDrawPixels(oldWidth, renderheight, GL_RGBA, GL_UNSIGNED_BYTE, scanptr);
	
	// Disable alpha test after blit:
    glDisable(GL_ALPHA_TEST);
	
    // Restore state:
    if(!PsychPrefStateGet_TextAlphaBlending()) PsychStoreAlphaBlendingFactorsForWindow(winRec, normalSourceBlendFactor, normalDestinationBlendFactor);
	
	glPopAttrib();
	
    // Mark end of drawing op. This is needed for single buffered drawing:
    PsychFlushGL(winRec);

	// We jump directly to this position if text appears to be completely outside the window:
drawtext_noop:

    // Update drawing cursor: Place cursor so that text could
    // be appended right-hand of the drawn text.
    // Get updated "cursor position":
	GetCurrentPositionEx(dc, &xy);
    *xp = xy.x;
    *yp = xy.y;
	
	// Restore to default font after text drawing:
	SelectObject(dc, defaultFont);
	
	// Done.
    return(PsychError_none);
}

// End of Windows specific part...
#endif

// End of non-OS/X (= Linux & Windows) specific part...
#endif

// Load and initialize an external text renderer plugin: Called while OpenGL
// context from 'windowRecord' is bound and active. Returns true on success,
// false on error. Reverts to builtin text renderer on error:
psych_bool PsychLoadTextRendererPlugin(PsychWindowRecordType* windowRecord)
{
#if PSYCH_SYSTEM != PSYCH_WINDOWS
	char pluginPath[FILENAME_MAX];

	// Assign name of plugin shared library based on target OS:
	#if PSYCH_SYSTEM == PSYCH_OSX
	// OS/X:
	char pluginName[] = "libptbdrawtext_ftgl.dylib";
	#else
	// Linux:
	char pluginName[] = "libptbdrawtext_ftgl.so.1";
	#endif

	// Try to load plugin if not already loaded: The dlopen call will search in all standard system
	// search paths, the $HOME/lib directory if any, and relative to the current working directory.
	// The functions in the plugin will be linked immediately and if successfull, made available
	// directly for use within the code, with no need to dlsym() manually bind'em:
	if (NULL == drawtext_plugin) {
		// Try to auto-detect install location of plugin inside the Psychtoolbox/PsychBasic folder.
		// If we manage to find the path to that folder, we can load with absolute path and thereby
		// don't need the plugin to be installed in a system folder -- No need for user to manually
		// install it, works plug & play :-)
		if (strlen(PsychRuntimeGetPsychtoolboxRoot()) > 0) {
			// Yes! Assemble full path name to plugin:
			sprintf(pluginPath, "%s/PsychBasic/PsychPlugins/%s", PsychRuntimeGetPsychtoolboxRoot(), pluginName);
			if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: DrawText: Trying to load external text renderer plugin from following file: [ %s ]\n", pluginPath);
		}
		else {
			// Failed! Assign only plugin name and hope the user installed the plugin into
			// a folder on the system library search path:
			sprintf(pluginPath, "%s", pluginName);
			if (PsychPrefStateGet_Verbosity() > 1) printf("PTB-WARNING: DrawText: Failed to find installation directory for text renderer plugin [ %s ].\nHoping it is somewhere in the library search path...\n", pluginPath);
		}
		
		drawtext_plugin = dlopen(pluginPath, RTLD_NOW | RTLD_GLOBAL);
		if (NULL == drawtext_plugin) {
			// First try failed:
			if (PsychPrefStateGet_Verbosity() > 1) {
				printf("PTB-WARNING: DrawText: Failed to load external drawtext plugin [%s]. Retrying under generic name [%s].\n", (const char*) dlerror(), pluginName);
			}

			sprintf(pluginPath, "%s", pluginName);
			drawtext_plugin = dlopen(pluginPath, RTLD_NOW | RTLD_GLOBAL);
		}
		drawtext_plugin_firstcall = TRUE;
	}
	else {
		drawtext_plugin_firstcall = FALSE;
	}

	// Successfully loaded and bound?
	if (NULL == drawtext_plugin) {
		// Failed! Revert to standard text rendering code below:
		if (PsychPrefStateGet_Verbosity() > 1) {
			printf("PTB-WARNING: DrawText: Failed to load external drawtext plugin [%s]. Reverting to standard text renderer.\n", (const char*) dlerror());
			printf("PTB-WARNING: DrawText: Functionality of Screen('DrawText') and Screen('TextBounds') may be limited and text quality may be impaired.\n");
			printf("PTB-WARNING: DrawText: Type 'help DrawTextPlugin' at the command prompt to receive instructions for troubleshooting.\n\n");
		}

		// Switch to renderer zero, which is the fast fallback renderer on all operating systems:
		PsychPrefStateSet_TextRenderer(0);
		
		// Return failure code:
		return(FALSE);
	}

	// Plugin loaded. Perform first time init, if needed:
	if (drawtext_plugin_firstcall) {

		// Dynamically bind all functions to their proper plugin entry points:
		PsychPluginInitText = dlsym(drawtext_plugin, "PsychInitText");
		PsychPluginShutdownText = dlsym(drawtext_plugin, "PsychShutdownText");
		PsychPluginRebuiltFont = dlsym(drawtext_plugin, "PsychRebuiltFont");
		PsychPluginSetTextFont = dlsym(drawtext_plugin, "PsychSetTextFont");
		PsychPluginSetTextStyle = dlsym(drawtext_plugin, "PsychSetTextStyle");
		PsychPluginSetTextSize = dlsym(drawtext_plugin, "PsychSetTextSize");
		PsychPluginSetTextFGColor = dlsym(drawtext_plugin, "PsychSetTextFGColor");
		PsychPluginSetTextBGColor = dlsym(drawtext_plugin, "PsychSetTextBGColor");
		PsychPluginSetTextUseFontmapper = dlsym(drawtext_plugin, "PsychSetTextUseFontmapper");
		PsychPluginSetTextViewPort = dlsym(drawtext_plugin, "PsychSetTextViewPort");
		PsychPluginDrawText = dlsym(drawtext_plugin, "PsychDrawText");
		PsychPluginMeasureText = dlsym(drawtext_plugin, "PsychMeasureText");
		PsychPluginSetTextVerbosity = dlsym(drawtext_plugin, "PsychSetTextVerbosity");
		PsychPluginSetTextAntiAliasing = dlsym(drawtext_plugin, "PsychSetTextAntiAliasing");
		
		// Assign current level of verbosity:
		PsychPluginSetTextVerbosity((unsigned int) PsychPrefStateGet_Verbosity());

		// Try to initialize plugin:
		if (PsychPluginInitText()) PsychErrorExitMsg(PsychError_internal, "Drawtext plugin, PsychInitText() failed!");
		
		// Enable use of plugins internal fontMapper for selection of font file, face type and rendering
		// parameters, based on the font/text spec provided by us:
		PsychPluginSetTextUseFontmapper(1, 0);
	}

#else
	if (PsychPrefStateGet_Verbosity() > 1) printf("PTB-WARNING: DrawText: Failed to load external drawtext plugin [Not supported on MS-Windows yet]. Reverting to standard text renderer.\n");
	
	// Switch to renderer one, which is the default renderer on Windows:
	PsychPrefStateSet_TextRenderer(1);
	
	// Return failure:
	return(FALSE);
#endif

	// Return success:
	return(TRUE);
}

// Common cleanup routine for all text renderers: Called from PsychCloseWindow() during
// window destruction while OpenGL context of 'windowRecord' is bound and active. Has to
// decide if any ressource cleanup work for window(s) has to be done and call into the
// OS/Engine specific cleanup routines:
void PsychCleanupTextRenderer(PsychWindowRecordType* windowRecord)
{
	// Do we have allocated display lists for the display list renderers on MS-Windows or Linux
	// for this onscreen window?
	if (windowRecord->textAttributes.DisplayList > 0) {
		// Yep. Destroy them:
		if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: In PsychCleanupTextRenderer: Releasing display list text renderer resources for window %i.\n", windowRecord->windowIndex);
		glDeleteLists(windowRecord->textAttributes.DisplayList, 256);
		windowRecord->textAttributes.DisplayList=0;
	}
	
	// Is this the last open onscreen window about to be destroyed, ie., after closing this one, will there be
	// no further onscreen windows?
	if ((PsychCountOpenWindows(kPsychDoubleBufferOnscreen) + PsychCountOpenWindows(kPsychSingleBufferOnscreen)) == 1) {
		// Yes. Time to shutdown the text renderer(s) and release all associated resources:
		#if PSYCH_SYSTEM == PSYCH_WINDOWS
			// Release GDI based MS-Windows text renderer:
			CleanupDrawtextGDI();
		#endif

		// Do we have an external text rendering plugin installed and initialized?
		if (drawtext_plugin) {
			// Yes.
			if (PsychPrefStateGet_Verbosity() > 5) printf("PTB-DEBUG: In PsychCleanupTextRenderer: Releasing text renderer plugin.\n");

			// Call plugin shutdown routine:
			PsychPluginShutdownText();

			#if PSYCH_SYSTEM != PSYCH_WINDOWS
			// Jettison plugin:
			dlclose(drawtext_plugin);
			#endif
			
			drawtext_plugin = NULL;
		}
	}

	return;
}

#if PSYCH_SYSTEM == PSYCH_WINDOWS
// MS-Windows:
#include <locale.h>

// The usual ugliness: When building against R11, we don't have support for
// _locale_t datatype and associated functions like mbstowcs_l. Therefore
// we use setlocale() and mbstowcs() instead to set/query/use the global
// process-wide locale instead. This could possibly wreak-havoc with Matlabs
// own locale processing. Otoh, only R2006b became locale aware, earlier releases
// don't use locales at all, and later releases R2007a et al. use the non R11
// build procedure where everything is fine.
// -> If there is potential for trouble at all, then probably only for
// R2006b. Cross your fingers...

#if defined(MATLAB_R11) || defined(PTBOCTAVE3MEX)
#define _locale_t	char*
#endif

static	_locale_t	drawtext_locale = NULL;
static	char		drawtext_localestring[256] = { 0 };
unsigned int		drawtext_codepage = 0;

// Define mbstowcs_l (Posix) to the corresponding name on Windows CRT:
#define	mbstowcs_l	_mbstowcs_l

// PsychSetUnicodeTextConversionLocale():
//
// Set the character encoding locale setting which is used by PsychAllocInTextAsUnicode(),
// e.g., when Screen('DrawText') is called with a char() string argument.
//
// The locale setting defines how to map the given (multibyte-)sequence of byte-characters
// into unicode codepoints, i.e., how the given string is translated into unicode.
//
// 'mnewlocale' can be one of the following:
// NULL -- Shutdown conversion routines, free all associated resources. Called at Screen exit time.
// ""   -- An empty string: Set locale to the system default locale, as defined by system settings or
//         environment variables at application startup time. E.g., $LANG, $LC_CTYPE, $LC_ALL on Posix...
// "xx" -- Some text string with the name of a locale supported by the system, e.g., "C" for default C
//         language locale, "en_US.ISO8859-1" for ISO8859-1 (Latin-1) encoding, "UTF-8" for UTF-8 multibyte
//         unicode encoding. On Unix, see "man xlocale", "man multibyte" for explanation.
//         See e.g., <http://msdn.microsoft.com/en-us/library/wyzd2bce(VS.80).aspx> for locale support on
//         MS-Windows.
//
// Returns TRUE on success, FALSE on error.
psych_bool	PsychSetUnicodeTextConversionLocale(const char* mnewlocale)
{
	unsigned int	mycodepage;

#if !defined(MATLAB_R11) && !defined(PTBOCTAVE3MEX)
	_locale_t		myloc = NULL;

	// Was only destruction/release of current locale requested?
	if (NULL == mnewlocale) {
		// Destroy/Release old locale, if any:
		if (drawtext_locale) {
			_free_locale(drawtext_locale);
			drawtext_locale = NULL;
		}
		drawtext_localestring[0] = 0;
		drawtext_codepage = 0;
		
		// Done after destruction:
		return(TRUE);
	}

	// Special # symbol to directly set a codepage provided?
	if (strstr(mnewlocale, "#") && (sscanf("#%i", &mycodepage) > 0)) {
		// Yes, parse numeric codepage id and assign it:
		strcpy(drawtext_localestring, mnewlocale);
		drawtext_codepage = mycodepage;
		return(TRUE);
	}
	else {
		// Special case "UTF-8" string provided?
		if (PsychMatch(mnewlocale, "UTF-8")) {
			// Yes: Switch to UTF-8 codepage:
			strcpy(drawtext_localestring, mnewlocale);
			drawtext_codepage = CP_UTF8;
			return(TRUE);
		}
	}
	
	// Setting of a new locale requested:
	myloc = _create_locale(LC_CTYPE, mnewlocale);
	if (myloc) {
		// Destroy/Release old locale, if any:
		if (drawtext_locale) {
			_free_locale(drawtext_locale);
			drawtext_locale = NULL;
		}

		drawtext_codepage = 0;
		drawtext_localestring[0] = 0;
		if (strlen(mnewlocale) < 1) {
			// Special locale "" given: Set namestring to current system
			// default locale:
			strcpy(drawtext_localestring, setlocale(LC_CTYPE, NULL));
		}
		else {
			// Named locale given: Assign its namestring:
			strcpy(drawtext_localestring, mnewlocale);
		}
		drawtext_locale = myloc;
		
		return(TRUE);
	}
	
	// Failed! No settings changed:
	return(FALSE);
#else
	// Was only destruction/release of current locale requested?
	if (NULL == mnewlocale) {
		// Revert process global locale to system default:
		setlocale(LC_CTYPE, "");
		drawtext_codepage = 0;
		return(TRUE);
	}

	// Special # symbol to directly set a codepage provided?
	if (strstr(mnewlocale, "#") && (sscanf("#%i", &mycodepage) > 0)) {
		// Yes, parse numeric codepage id and assign it:
		strcpy(drawtext_localestring, mnewlocale);
		drawtext_codepage = mycodepage;
		return(TRUE);
	}
	else {
		// Special case "UTF-8" string provided?
		if (PsychMatch(mnewlocale, "UTF-8")) {
			// Yes: Switch to UTF-8 codepage:
			strcpy(drawtext_localestring, mnewlocale);
			drawtext_codepage = CP_UTF8;
			return(TRUE);
		}
	}
	
	// Setting of a new locale requested: Try to set it globally for the
	// whole process, return success status:
	drawtext_codepage = 0;
	return( (setlocale(LC_CTYPE, mnewlocale) == NULL) ? FALSE : TRUE );
#endif
}

// PsychGetUnicodeTextConversionLocale(): 
//
// Get the character encoding locale setting string which is used by PsychAllocInTextAsUnicode(),
// e.g., when Screen('DrawText') is called with a char() string argument.
//
// Returns NULL on error, a const char* string with the current locale setting on success.
const char* PsychGetUnicodeTextConversionLocale(void)
{
#if !defined(MATLAB_R11) && !defined(PTBOCTAVE3MEX)
	return(&drawtext_localestring[0]);
#else
	// Return encoded codepage:
	if (drawtext_codepage) return(&drawtext_localestring[0]);

	// Query process global locale:
	return(setlocale(LC_CTYPE, NULL));
#endif
}

#else

// POSIX systems Linux and OS/X:
#include <locale.h>
#include <xlocale.h>

static	locale_t	drawtext_locale = NULL;
static	char		drawtext_localestring[256] = { 0 };

#if PSYCH_SYSTEM == PSYCH_LINUX

size_t mbstowcs_l(wchar_t *dest, const char *src, size_t n, locale_t theLocale);

/* mbstowcs_l reimplementation, because mbstowcs_l is only supported on OS/X,
 * not on Linux :-(
 */
size_t mbstowcs_l(wchar_t *dest, const char *src, size_t n, locale_t theLocale)
{
	size_t		rcsize;
	locale_t	oldloc;

	// Query current locale of this thread, store it in oldloc, set new
	// locale theLocale:
	oldloc = uselocale(theLocale);
	// Execute mbstowcs with theLocale assigned:
	rcsize = mbstowcs(dest, src, n);
	// Restore old locale setting oldloc:
	uselocale(oldloc);
	// Return whatever mbstowcs returned:
	return(rcsize);
}

#endif

psych_bool	PsychSetUnicodeTextConversionLocale(const char* mnewlocale)
{
	locale_t		myloc = NULL;

	// Was only destruction/release of current locale requested?
	if (NULL == mnewlocale) {
		// Destroy/Release old locale, if any:
		if (drawtext_locale) {
			freelocale(drawtext_locale);
			drawtext_locale = NULL;
		}

		drawtext_localestring[0] = 0;
		
		// Done after destruction:
		return(TRUE);
	}
	
	// Setting of a new locale requested:
	myloc = newlocale(LC_CTYPE_MASK, mnewlocale, NULL);
	if (myloc) {
		// Destroy/Release old locale, if any:
		if (drawtext_locale) {
			freelocale(drawtext_locale);
			drawtext_locale = NULL;
		}
		drawtext_localestring[0] = 0;

		if (strlen(mnewlocale) < 1) {
			// Special locale "" given: Set namestring to current system
			// default locale:
			strcpy(drawtext_localestring, setlocale(LC_CTYPE, NULL));
		}
		else {
			// Named locale given: Assign its namestring:
			strcpy(drawtext_localestring, mnewlocale);
		}

		drawtext_locale = myloc;
		
		return(TRUE);
	}
	
	// Failed! No settings changed:
	return(FALSE);
}

const char* PsychGetUnicodeTextConversionLocale(void)
{
	#if PSYCH_SYSTEM == PSYCH_OSX
	return(querylocale(LC_CTYPE_MASK, drawtext_locale));
	#else
	return(&drawtext_localestring[0]);
	#endif
}

#endif

// Allocate in a text string argument, either in some string or bytestring format or as double-vector.
// Return the strings representation as a double vector in Unicode encoding.
//
// 'position' the position of the string argument.
// 'isRequired' Is the string required or optional, or required to be of a specific type?
// 'textLength' On return, store length of text string in characters at the int pointer target location.
// 'unicodeText' On return, the double* to which unicodeText points, shall contain the start adress of a vector of
//               doubles. Each double encodes the unicode value of one unicode character in the string. Length of
//               the array as given in 'textLength'.
//
//  Returns TRUE on successfull allocation of an input string in unicode format. FALSE on any error.
//
psych_bool	PsychAllocInTextAsUnicode(int position, PsychArgRequirementType isRequired, int *textLength, double **unicodeText)
{
	int				dummy1, dummy2;
	unsigned char	*textByteString = NULL;
    char			*textCString = NULL;
	wchar_t			*textUniString = NULL;
	int				stringLengthBytes = 0;

	// Anything provided as argument? This checks for presence of the required arg. If an arg
	// of mismatching type (not char or double) is detected, it errors-out. Otherwise it returns
	// true on presence of a correct argument, false if argument is absent and optional.
	if (!PsychCheckInputArgType(position, isRequired, (PsychArgType_char | PsychArgType_double | PsychArgType_uint8))) {
		// The optional argument isn't present. That means there ain't any work for us to do:
		goto allocintext_skipped;
	}

	// Some text string available, either double vector or char vector.
	
	// Text string at 'position' passed as C-language encoded character string or string of uint8 bytes?
    if ((PsychGetArgType(position) == PsychArgType_char) || (PsychGetArgType(position) == PsychArgType_uint8)) {
		// Try to allocate in as unsigned byte string:
		if (PsychAllocInUnsignedByteMatArg(position, kPsychArgAnything, &dummy1, &stringLengthBytes, &dummy2, &textByteString)) {
			// Yep: Convert to null-terminated string for further processing:
			if (dummy2!=1) PsychErrorExitMsg(PsychError_user, "Byte text matrices must be 2D matrices!");
			stringLengthBytes = stringLengthBytes * dummy1;
			
			// Nothing to do on empty string:
			if (stringLengthBytes < 1 || textByteString[0] == 0) goto allocintext_skipped;
			
			// A bytestring. Is it null-terminated? If not we need to make it so:
			if (textByteString[stringLengthBytes-1] != 0) {
				// Not null-terminated: Create a 1 byte larger temporary copy which is null-terminated:
				textCString = (char*) PsychMallocTemp(stringLengthBytes + 1);
				memcpy((void*) textCString, (void*) textByteString, stringLengthBytes);
				textCString[stringLengthBytes] = 0;
			}
			else {
				// Already null-terminated: Nice :-)
				textCString = (char*) textByteString;
			}
		}
		else {
			// Null terminated C-Language text string ie., a sequence of bytes. Get it:
			PsychAllocInCharArg(position, TRUE, &textCString);
		}
		
		// Get length in bytes, derived from location of null-terminator character:
		stringLengthBytes = strlen(textCString);
		
		// Empty string? If so, we skip processing:
		if (stringLengthBytes < 1) goto allocintext_skipped;
		
		#if PSYCH_SYSTEM == PSYCH_WINDOWS
			// Windows:
			// Compute number of Unicode wchar_t chars after conversion of multibyte C-String:
			if (drawtext_codepage) {
				// Codepage-based text conversion:
				*textLength = MultiByteToWideChar(drawtext_codepage, 0, textCString, -1, NULL, 0) - 1;
				if (*textLength <= 0) {
					printf("PTB-ERROR: MultiByteToWideChar() returned conversion error code %i.", (int) GetLastError());
					PsychErrorExitMsg(PsychError_user, "Invalid multibyte character sequence detected! Can't convert given char() string to Unicode for DrawText!");
				}
			}
			else {
				// Locale-based text conversion:
				#if defined(MATLAB_R11) || defined(PTBOCTAVE3MEX)
						*textLength = mbstowcs(NULL, textCString, 0);
				#else
						*textLength = mbstowcs_l(NULL, textCString, 0, drawtext_locale);
				#endif
			}
		#else
			// Unix: OS/X, Linux:
			*textLength = mbstowcs_l(NULL, textCString, 0, drawtext_locale);
		#endif
		
		if (*textLength < 0) PsychErrorExitMsg(PsychError_user, "Invalid multibyte character sequence detected! Can't convert given char() string to Unicode for DrawText!");
		
		// Empty string provided? Skip, if so.
		if (*textLength < 1) goto allocintext_skipped;
		
		// Allocate wchar_t buffer of sufficient size to hold converted unicode string:
		textUniString = (wchar_t*) PsychMallocTemp((*textLength + 1) * sizeof(wchar_t));
		
		// Perform conversion of multibyte character sequence to Unicode wchar_t:
		#if PSYCH_SYSTEM == PSYCH_WINDOWS
			// Windows:
			if (drawtext_codepage) {
				// Codepage-based text conversion:
				if (MultiByteToWideChar(drawtext_codepage, 0, textCString, -1, textUniString, (*textLength + 1)) <= 0) {
					printf("PTB-ERROR: MultiByteToWideChar() II returned conversion error code %i.", (int) GetLastError());
					PsychErrorExitMsg(PsychError_user, "Invalid multibyte character sequence detected! Can't convert given char() string to Unicode for DrawText!");
				}
			}
			else {
				// Locale-based text conversion:
				#if defined(MATLAB_R11) || defined(PTBOCTAVE3MEX)
					mbstowcs(textUniString, textCString, (*textLength + 1));
				#else
					mbstowcs_l(textUniString, textCString, (*textLength + 1), drawtext_locale);
				#endif
			}
		#else
			// Unix:
			mbstowcs_l(textUniString, textCString, (*textLength + 1), drawtext_locale);			
		#endif
		
		// Allocate temporary output vector of doubles and copy unicode string into it:
		*unicodeText = (double*) PsychMallocTemp((*textLength + 1) * sizeof(double));
		for (dummy1 = 0; dummy1 < (*textLength + 1); dummy1++) (*unicodeText)[dummy1] = (double) textUniString[dummy1];
	}
	else {
		// Not a character string: Check if it is a double matrix which directly encodes Unicode text:
		PsychAllocInDoubleMatArg(position, TRUE, &dummy1, &stringLengthBytes, &dummy2, unicodeText);
		if (dummy2!=1) PsychErrorExitMsg(PsychError_user, "Unicode text matrices must be 2D matrices!");
		stringLengthBytes = stringLengthBytes * dummy1;
		
		// Empty string? If so, we skip processing:
		if(stringLengthBytes < 1) goto allocintext_skipped;

		// Nope. Assign output arguments. We can pass-through the unicode double vector as it is
		// already in the proper format:
		*textLength = stringLengthBytes;
	}

	if (PsychPrefStateGet_Verbosity() > 9) {
		printf("PTB-DEBUG: Allocated unicode string: ");
		for (dummy1 = 0; dummy1 < *textLength; dummy1++) printf("%f ", (float) (*unicodeText)[dummy1]);	
		printf("\n");
	}

	// Successfully allocated a text string as Unicode double vector:
	return(TRUE);

// We reach this jump-label via goto if there isn't any text string to return:
allocintext_skipped:
	*textLength = 0;
	*unicodeText = NULL;
	return(FALSE);
}

void PsychDrawCharText(PsychWindowRecordType* winRec, const char* textString, double* xp, double* yp, unsigned int yPositionIsBaseline, PsychColorType *textColor, PsychColorType *backgroundColor, PsychRectType* boundingbox)
{
	// Convert textString to Unicode format double vector:
	int ix;
	unsigned int textLength = (unsigned int) strlen(textString);
	double* unicodeText = (double*) PsychCallocTemp(textLength + 1, sizeof(double));
	for (ix = 0; ix < textLength; ix++) unicodeText[ix] = (double) textString[ix];
	
	// Call Unicode text renderer:
	PsychDrawUnicodeText(winRec, boundingbox, textLength, unicodeText, xp, yp, yPositionIsBaseline, (textColor) ? textColor :  &(winRec->textAttributes.textColor), (backgroundColor) ? backgroundColor :  &(winRec->textAttributes.textBackgroundColor), 0);

	// Done.
	return;
}

PsychError PsychDrawUnicodeText(PsychWindowRecordType* winRec, PsychRectType* boundingbox, unsigned int stringLengthChars, double* textUniDoubleString, double* xp, double* yp, unsigned int yPositionIsBaseline, PsychColorType *textColor, PsychColorType *backgroundColor, int swapTextDirection)
{
	GLdouble		backgroundColorVector[4];
	GLdouble		colorVector[4];
    GLenum			normalSourceBlendFactor, normalDestinationBlendFactor;
	float			xmin, ymin, xmax, ymax;
	double			myyp;
	double			dummy;
	int				i;
	int				rc = 0;

	// Invert text string (read it "backwards") if swapTextDirection is requested:
	if (swapTextDirection) {
		for(i = 0; i < stringLengthChars/2; i++) {
			dummy = textUniDoubleString[i];
			textUniDoubleString[i] = textUniDoubleString[stringLengthChars - i - 1];
			textUniDoubleString[stringLengthChars - i - 1] = dummy;
		}
	}
	
	// Does usercode want us to use a text rendering plugin instead of our standard OS specific renderer?
	// If so, load it if not already loaded:
	if (((PsychPrefStateGet_TextRenderer() == 2) || ((PsychPrefStateGet_TextRenderer() == 1) && (PSYCH_SYSTEM == PSYCH_LINUX))) &&	
	    PsychLoadTextRendererPlugin(winRec)) {

		// Use external dynamically loaded plugin:

		// Assign current level of verbosity:
		PsychPluginSetTextVerbosity((unsigned int) PsychPrefStateGet_Verbosity());

		// Assign current anti-aliasing settings:
		PsychPluginSetTextAntiAliasing(PsychPrefStateGet_TextAntiAliasing());

		// Assign font family name of requested font:
		PsychPluginSetTextFont(winRec->textAttributes.textFontName);

		// Assign style settings, e.g., bold, italic etc.:
		PsychPluginSetTextStyle(winRec->textAttributes.textStyle);

		// Assign text size in pixels:
		PsychPluginSetTextSize((double) winRec->textAttributes.textSize);

		// Assign viewport settings for rendering:
		PsychPluginSetTextViewPort(winRec->rect[kPsychLeft], winRec->rect[kPsychTop], winRec->rect[kPsychRight] - winRec->rect[kPsychLeft], winRec->rect[kPsychBottom] - winRec->rect[kPsychTop]);	
		
		// Compute and assign text background color:
		PsychConvertColorToDoubleVector(backgroundColor, winRec, backgroundColorVector);
		PsychPluginSetTextBGColor(backgroundColorVector);
		
		// Compute and assign text foreground color - the actual color of the glyphs:
		PsychConvertColorToDoubleVector(textColor, winRec, colorVector);
		PsychPluginSetTextFGColor(colorVector);
		
		// Enable this windowRecords framebuffer as current drawingtarget:
		PsychSetDrawingTarget(winRec);
		
		// Save all state:
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		
		// Disable draw shader:
		PsychSetShader(winRec, 0);
		
		// Override current alpha blending settings to GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA unless
		// usercode explicitely requested to use the regular Screen('Blendfunction') settings.
		// This is needed to perform proper text anti-aliasing via alpha-blending:
		if (!PsychPrefStateGet_TextAlphaBlending()) {
			PsychGetAlphaBlendingFactorsFromWindow(winRec, &normalSourceBlendFactor, &normalDestinationBlendFactor);
			PsychStoreAlphaBlendingFactorsForWindow(winRec, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		
		// Apply blending settings:
		PsychUpdateAlphaBlendingFactorLazily(winRec);
		
		// Disable apple client storage - it could interfere:
		#if PSYCH_SYSTEM == PSYCH_OSX
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
		#endif
		
		// Compute bounding box of drawn string:
		rc = PsychPluginMeasureText(stringLengthChars, textUniDoubleString, &xmin, &ymin, &xmax, &ymax);

		// Handle definition of yp properly: Is it the text baseline, or the top of the text bounding box?
		if (yPositionIsBaseline) {
			myyp = *yp;
		}
		else {
			myyp = *yp + ymax;
		}

		// Only bounding box requested?
		if (boundingbox) {
			// Yes. Return it:
			PsychMakeRect(boundingbox, xmin + *xp, myyp - ymax, xmax + *xp, myyp - ymin);
		}
		else {
			// Draw text by calling into the plugin:
			rc += PsychPluginDrawText(*xp, winRec->rect[kPsychBottom] - myyp, stringLengthChars, textUniDoubleString);
		}
		
		// Restore alpha-blending settings if needed:
		if (!PsychPrefStateGet_TextAlphaBlending()) PsychStoreAlphaBlendingFactorsForWindow(winRec, normalSourceBlendFactor, normalDestinationBlendFactor);

		// Restore GL state:
		glPopAttrib();
		
		// Mark end of drawing op. This is needed for single buffered drawing:
		PsychFlushGL(winRec);
		
		// Plugin rendering successfull?
		if (0 == rc) {
			// Yes. Update x position of text drawing cursor:
			*xp = *xp + (xmax - xmin + 1);
			
			// Return control to calling function:
			return(PsychError_none);
		}
		
		// If we reach this point then the plugin failed to render text:
		PsychErrorExitMsg(PsychError_user, "The external text renderer plugin failed to render the text string for some reason!");
	}
	
	// If we reach this point then either text rendering via OS specific renderer is requested, or
	// the external rendering plugin failed to load and we use the OS specific renderer as fallback.
	return(PsychOSDrawUnicodeText(winRec, boundingbox, stringLengthChars, textUniDoubleString, xp, yp, yPositionIsBaseline, textColor, backgroundColor));
}


// Unified 'DrawText' routine, as called by Screen('DrawText', ...);
PsychError SCREENDrawText(void)
{
    PsychWindowRecordType	*winRec;
    psych_bool				doSetColor, doSetBackgroundColor;
    PsychColorType			colorArg, backgroundColorArg;
    int						i, yPositionIsBaseline, swapTextDirection;
    int						stringLengthChars;
	double*					textUniDoubleString = NULL;

    // All subfunctions should have these two lines.  
    PsychPushHelp(useString, synopsisString, seeAlsoString);
    if (PsychIsGiveHelp()) { PsychGiveHelp(); return(PsychError_none); };

    PsychErrorExit(PsychCapNumInputArgs(8));   	
    PsychErrorExit(PsychRequireNumInputArgs(2)); 	
    PsychErrorExit(PsychCapNumOutputArgs(2));  

    //Get the window structure for the onscreen window.
    PsychAllocInWindowRecordArg(1, TRUE, &winRec);
	
	// Check if input text string is present, valid and non-empty, get it as double vector
	// of unicode characters: If this returns false then there ain't any work for us to do:
	if(!PsychAllocInTextAsUnicode(2, kPsychArgRequired, &stringLengthChars, &textUniDoubleString)) goto drawtext_skipped;

    // Get the X and Y positions.
    PsychCopyInDoubleArg(3, kPsychArgOptional, &(winRec->textAttributes.textPositionX));
    PsychCopyInDoubleArg(4, kPsychArgOptional, &(winRec->textAttributes.textPositionY));
    
    //Get the new color record, coerce it to the correct mode, and store it.  
    doSetColor = PsychCopyInColorArg(5, kPsychArgOptional, &colorArg);
    if (doSetColor) PsychSetTextColorInWindowRecord(&colorArg, winRec);

    // Same for background color:
	doSetBackgroundColor = PsychCopyInColorArg(6, kPsychArgOptional, &backgroundColorArg);
	if (doSetBackgroundColor) PsychSetTextBackgroundColorInWindowRecord(&backgroundColorArg, winRec);

	// Special handling of offset for y position correction:
	yPositionIsBaseline = PsychPrefStateGet_TextYPositionIsBaseline();
	PsychCopyInIntegerArg(7, kPsychArgOptional, &yPositionIsBaseline);

	// Get optional text writing direction flag: Defaults to left->right aka 0:
	swapTextDirection = 0;
	PsychCopyInIntegerArg(8, kPsychArgOptional, &swapTextDirection);
	
	// Call Unicode text renderer: This will update the current text cursor positions as well.
	PsychDrawUnicodeText(winRec, NULL, stringLengthChars, textUniDoubleString, &(winRec->textAttributes.textPositionX), &(winRec->textAttributes.textPositionY), yPositionIsBaseline, &(winRec->textAttributes.textColor), &(winRec->textAttributes.textBackgroundColor), swapTextDirection);

	// We jump directly to this position in the code if the textstring is empty --> No op.
drawtext_skipped:    

    // Copy out new, potentially updated, "cursor position":
    PsychCopyOutDoubleArg(1, FALSE, winRec->textAttributes.textPositionX);
    PsychCopyOutDoubleArg(2, FALSE, winRec->textAttributes.textPositionY);
    
	// Done.
    return(PsychError_none);
}
