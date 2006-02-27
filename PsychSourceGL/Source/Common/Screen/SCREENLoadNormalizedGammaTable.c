/*

	Psychtoolbox3/Source/Common/SCREENLoadNormalizedGammaTable.c		

  

	AUTHORS:



		Allen.Ingling@nyu.edu		awi 

  

	PLATFORMS:	

	

		Only OS X for now.



	HISTORY:



		1/27/03  awi		Created. 
      1/30/06  mk       Improved online help text.
 

	DESCRIPTION:

	Interface to the operating systems hardware gamma-table functions.    

	TO DO:  

*/





#include "Screen.h"



// If you change useString then also change the corresponding synopsis string in ScreenSynopsis.c

static char useString[] = "Screen('LoadNormalizedGammaTable', windowPtrOrScreenNumber, table);";

static char synopsisString[] = 

        "Load the gamma table of the specified screen or windowPtr. You need to pass the new "
			"hardware gamma table 'table' as a 256 rows by 3 columns matrix. Each row corresponds to "
			"a single color index value in the framebuffer and contains the Red- green- and blue values "
			"to use for output. Column 1 is the red value, column 2 is the green value and column 3 is "
			"the blue value. Values have to be in range between 0.0 (for dark pixel) and 1.0 (for maximum intensity). "
			"Example: table(127,1)=0.67 would mean that the red color value 127 should be displayed with 67% of "
         "the maximum red-gun intensity, table(32, 3)=0.11 means that blue color value 32 should be displayed "
			"with 11% of the maximum blue-gun intensity. The range of values 0-1 gets mapped to the hardware with "
			"the accuracy attainable by the hardwares DAC's, typically between 8 and 10 bits. "
			"On MacOS-X, this function takes arbitrary gamma-tables which makes it suitable for fast CLUT animation. "
			"On Microsoft Windows, only tables with monotonically increasing values are considered valid. Other tables "
			"get rejected by the operating system -- there's nothing we can do about this incredibly wise decision "
			"of the Microsoft system designers :( , so this is not suitable for CLUT animation, but only for linearizing "
			"or calibrating display devices. "
			"The function returns the old gamma table as optional return argument. ";

static char seeAlsoString[] = "ReadNormalizedGammaTable";

PsychError SCREENLoadNormalizedGammaTable(void) 
{
    int		i, screenNumber, numEntries, inM, inN, inP;
    float 	*outRedTable, *outGreenTable, *outBlueTable, *inRedTable, *inGreenTable, *inBlueTable;
    double	 *inTable, *outTable;	

    //all subfunctions should have these two lines
    PsychPushHelp(useString, synopsisString, seeAlsoString);
    if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};

    PsychErrorExit(PsychCapNumOutputArgs(1));
    PsychErrorExit(PsychCapNumInputArgs(2));

    //read in the screen number
    PsychCopyInScreenNumberArg(1, TRUE, &screenNumber);
    
    //load, sanity check the input matrix, and covert from float to doubles
    PsychAllocInDoubleMatArg(2, TRUE, &inM,  &inN, &inP, &inTable);

    if((inM != 256) || (inN != 3) || (inP != 1))
        PsychErrorExitMsg(PsychError_user, "The gamma table must be 256x3");

    inRedTable=mxMalloc(sizeof(float) * 256);
    inGreenTable=mxMalloc(sizeof(float) * 256);
    inBlueTable=mxMalloc(sizeof(float) * 256);

    for(i=0;i<256;i++){
        inRedTable[i]=(float)inTable[PsychIndexElementFrom3DArray(256, 3, 0, i, 0, 0)];
        inGreenTable[i]=(float)inTable[PsychIndexElementFrom3DArray(256, 3, 0, i, 1, 0)];
        inBlueTable[i]=(float)inTable[PsychIndexElementFrom3DArray(256, 3, 0, i, 2, 0)];

        if(inRedTable[i]>1 || inRedTable[i]< 0 || inGreenTable[i] > 1 || inGreenTable[i] < 0 || inBlueTable[i] >1 || inBlueTable[i] < 0)
            PsychErrorExitMsg(PsychError_user, "Gamma Table Values must be in interval 0 =< x =< 1");
    }

    //first read the existing gamma table so we can return it.  
    PsychReadNormalizedGammaTable(screenNumber, &numEntries, &outRedTable, &outGreenTable, &outBlueTable);
    PsychAllocOutDoubleMatArg(1, FALSE, numEntries, 3, 0, &outTable);

    for(i=0;i<numEntries;i++){
        outTable[PsychIndexElementFrom3DArray(numEntries, 3, 0, i, 0, 0)]=(double)outRedTable[i];
        outTable[PsychIndexElementFrom3DArray(numEntries, 3, 0, i, 1, 0)]=(double)outGreenTable[i];
        outTable[PsychIndexElementFrom3DArray(numEntries, 3, 0, i, 2, 0)]=(double)outBlueTable[i];
    }

    //Now set the new gamma table
    PsychLoadNormalizedGammaTable(screenNumber, numEntries, inRedTable, inGreenTable, inBlueTable);

    return(PsychError_none);
}
