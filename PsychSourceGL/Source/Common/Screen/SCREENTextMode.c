/*
	SCREENTextMode.c	
  
	AUTHORS:

		Allen.Ingling@nyu.edu			awi
		mario.kleiner@tuebingen.mpg.de	mk
  
	PLATFORMS:	
  
		All.
    
	HISTORY:

		11/18/03	awi		Wrote it.
		10/12/04	awi		In useString: changed "SCREEN" to "Screen", and moved commas to inside [].

	DESCRIPTION:
  
		Sets the text mode for the specified window record.
  
	NOTES:

*/


#include "Screen.h"

// If you change useString then also change the corresponding synopsis string in ScreenSynopsis.c
static char useString[] ="oldCopyMode=Screen('TextMode', windowPtr [,textMode]);";
//                                            0          1           2
static char synopsisString[] =	"Set or get the text mode for the specified window. This function currently hasn't "
								"any effect whatsoever. It is not even clear what kind of effects the different "
								"text modes should have or if this feature will be ever implemented.";
static char seeAlsoString[] = "TextModes";

PsychError SCREENTextMode(void) 
{

    PsychTextDrawingModeType		newCopyMode;
    Str255							oldCopyModeName; 
    char							*newCopyModeName;
    psych_bool							doSetMode;
    PsychWindowRecordType			*windowRecord;
    psych_bool							nameError;                           
    
    //all subfunctions should have these two lines.  
    PsychPushHelp(useString, synopsisString, seeAlsoString);
    if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};
    
    //check for valid number of arguments
    PsychErrorExit(PsychRequireNumInputArgs(1));
    PsychErrorExit(PsychCapNumInputArgs(2));   	
    PsychErrorExit(PsychCapNumOutputArgs(1)); 
    
    //Get the window record
    PsychAllocInWindowRecordArg(kPsychUseDefaultArgPosition, TRUE, &windowRecord);
    
    //Get the old copy mode & its name
    PsychGetTextDrawingModeNameFromTextDrawingModeConstant(oldCopyModeName, 255, windowRecord->textAttributes.textMode);
    PsychCopyOutCharArg(1, FALSE, oldCopyModeName);
    
    //Get the copy new mode string 
    doSetMode= PsychAllocInCharArg(2, FALSE, &newCopyModeName);
    if(doSetMode){
        nameError=PsychGetTextDrawingModeConstantFromTextDrawingModeName(&newCopyMode, newCopyModeName);
        if(nameError)
            PsychErrorExitMsg(PsychError_user, "Invalid text copy mode.  See Screen('TextModes') for a list of allowable modes");
		windowRecord->textAttributes.needsRebuild|=(windowRecord->textAttributes.textMode != newCopyMode) ? TRUE : FALSE;
        windowRecord->textAttributes.textMode=newCopyMode;	
    }
	
    return(PsychError_none);
}
