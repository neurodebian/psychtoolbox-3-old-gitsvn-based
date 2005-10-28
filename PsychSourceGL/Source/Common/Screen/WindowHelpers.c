/*
  PsychToolbox2/Source/Common/WindowHelpers.c		
  
  AUTHORS:
  Allen.Ingling@nyu.edu		awi 
  
  PLATFORMS: This file is included for all targets.
    
  PROJECTS:
  12/20/02	awi		Screen
   

  HISTORY:
  12/20/02  	awi	Wrote it.
  04/22/05      mk      Bugfix for PsychCheckIfWindowRecordIsValid: Screen('Close', w); Screen('Close', w) caused crash due to NULL-Ptr deref.
  
  DESCRIPTION:
  
  Convenience functions for extracting properties of windows from a window index or from a window record pointer.
  Sometimes doing that is a hassle because the properties of the window are actually properties of the associeate screen
  structure, hence we provide these conveience functions.
  
  TO DO:
  -fix Onscreen Windows so that the depth is valid and remove the conditional from
   PsychGetWindowDepthValueFromWindowRecord() 
  
   
*/

#include "Screen.h"


/*
	PsychGetColorSizeFromWindowRecord()
	
	Accepts a windows records and return the size in bits of a color plane. This assumes that all color planes have the same size.
	We might have to augment this to account for situations when this is not true.  
	
*/
int PsychGetColorSizeFromWindowRecord(PsychWindowRecordType *windowRecord)
{
    return(PsychGetColorSizeFromDepthValue(windowRecord->depth));
}



int PsychGetWindowDepthValueFromWindowRecord(PsychWindowRecordType *windowRecord)
{
    if(PsychIsOnscreenWindow(windowRecord)) 
        return(PsychGetScreenDepthValue(windowRecord->screenNumber));
    else if(PsychIsOffscreenWindow(windowRecord))
        return(windowRecord->depth);
    else{
        PsychErrorExit(PsychError_internal);
        return(0); //makes the compiler happy
    }
}

PsychColorModeType PsychGetColorModeFromWindowRecord(PsychWindowRecordType *windowRecord)
{
    return(PsychGetColorModeFromDepthValue(windowRecord->depth));
}


void PsychGetRectFromWindowRecord(double *rect, PsychWindowRecordType *windowRecord)
{
    
    PsychCopyRect(rect, windowRecord->rect);
}



int PsychGetNumPlanesFromWindowRecord(PsychWindowRecordType *windowRecord)
{
    return(PsychGetNumPlanesFromDepthValue(windowRecord->depth));
}

int PsychGetNumBuffersFromWindowRecord(PsychWindowRecordType *windowRecord)
{
        if(windowRecord->windowType==kPsychSingleBufferOnscreen)
            return(1);
        else if(windowRecord->windowType==kPsychDoubleBufferOnscreen)
            return(2);
        else if(windowRecord->windowType==kPsychVideoMemoryOffscreen)
            return(1);
        else if(windowRecord->windowType==kPsychSystemMemoryOffscreen)
            return(1);
        else{
            PsychErrorExitMsg(PsychError_internal, "illegitimate buffer type");
            return(0); //make the compiler happy
        }
}



void PsychSetWindowRecordValid(PsychWindowRecordType *winRec)
{
    winRec->isValid=TRUE;
}

void PsychCheckIfWindowRecordIsValid(PsychWindowRecordType *winRec)
{
    // MK: Added check for winRec==NULL.
    if(winRec==NULL || !winRec->isValid)
        PsychErrorExit(PsychError_InvalidWindowRecord);
}

boolean PsychIsOnscreenWindow(PsychWindowRecordType *windowRecord)
{
    return(windowRecord->windowType==kPsychSingleBufferOnscreen || windowRecord->windowType==kPsychDoubleBufferOnscreen);
        
}

/*
    PsychIsMatlabOnscreenWindow()
        
    stand-in for actual detection; No window records will are Matlab windows until we provide a way to 
    grab those, therefore return FALSE.    
*/
boolean PsychIsMatlabOnscreenWindow(PsychWindowRecordType *windowRecord)
{
    return(FALSE);
}


boolean PsychIsOffscreenWindow(PsychWindowRecordType *windowRecord)
{
    return(windowRecord->windowType==kPsychVideoMemoryOffscreen || windowRecord->windowType==kPsychSystemMemoryOffscreen);
}


boolean PsychIsTexture(PsychWindowRecordType *windowRecord)
{
    return(windowRecord->windowType==kPsychTexture);
}


void PsychSetTextColorInWindowRecord(PsychColorType *textColor,  PsychWindowRecordType *winRec)
{
	PsychColorModeType		colorMode;
	int						depth;
	
		
	colorMode=PsychGetColorModeFromWindowRecord(winRec);
	depth=PsychGetDepthFromWindowRecord(winRec);
	memcpy(&(winRec->textAttributes.textColor), textColor, sizeof(PsychColorType));
	PsychCoerceColorModeWithDepthValue(colorMode, depth, &(winRec->textAttributes.textColor));
	
}

void PsychSetTextBackgroundColorInWindowRecord(PsychColorType *textBackgroundColor,  PsychWindowRecordType *winRec)
{
	PsychColorModeType		colorMode;
	int						depth;
	
		
	colorMode=PsychGetColorModeFromWindowRecord(winRec);
	depth=PsychGetDepthFromWindowRecord(winRec);
	memcpy(&(winRec->textAttributes.textBackgroundColor), textBackgroundColor, sizeof(PsychColorType));
	PsychCoerceColorModeWithDepthValue(colorMode, depth, &(winRec->textAttributes.textBackgroundColor));
}


int PsychGetDepthFromWindowRecord(PsychWindowRecordType *winRec)
{
	return(winRec->depth);
}






