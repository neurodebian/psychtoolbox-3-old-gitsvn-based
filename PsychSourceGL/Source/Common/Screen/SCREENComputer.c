/*
	Psychtoolbox3/Source/Common/SCREENComputer.c		
  
	AUTHORS:

		Allen.Ingling@nyu.edu		awi 
  
	PLATFORMS:	
	
		Only OS X for now.
		
	HISTORY:

		4/01/03		awi		Created. 
		10/12/04	awi		Changed "SCREEN" to "Screen" in useString.
		10/21/04	awi		Added comment showing returned struct on OS X.
		1/11/04		awi		Fix bug in reporting memory size, reported by David Jones and uncovered in source by Mario Kleiner.
							Cleaned up ReportSysctlError.
		

 
	DESCRIPTION:
  
		Returns information about the computer. 
  
	TO DO:
  
		Macro out the parts specfic to OS X or abstract them.
		
		Add OS X and Linux flags to the OS 9 and Windows version of Screen 'Computer'
		
		Migrate this out of Screen.  It makes no sense for it to be here. Rename it to 
		"ComputerInfo".  We already have "FontInfo".  Generally, Psychtoolbox functions
		which return a struct with a bunch of info should be named *Info.     
*/



/* This is the struct returned by the OS9 version of SCREEN 'Computer'.

	macintosh: 1
	windows: 0
	emulating: ''
	pci: 1
	vm: ''
	busHz: 133216625
	hz: 999999997
	fpu: ''
	cache: ''
	processor: 'G4'
	system: 'Mac OS 9.2.2'
	owner: ''
	model: 'Classic Mac OS Compatibility/999'
*/


/*
from sys/time.h
struct clockinfo {
	int	hz;		// clock frequency
	int	tick;		// micro-seconds per hz tick
        int	tickadj;	// clock skew rate for adjtime() 
	int	stathz;		// statistics clock frequency 
	int	profhz;		// profiling clock frequency 
};
*/


#include "Screen.h"
//special includes for sysctl calls
#include <sys/types.h>
#include <sys/sysctl.h>
//special include for SCDynamicStoreCopySpecific* functions
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCDynamicStore.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
//for getting the ethernet MAC address
#include "GetEthernetAddress.h"


// If you change the useString then also change the corresponding synopsis string in ScreenSynopsis.c
static char useString[]= "comp=Screen('Computer')";
static char synopsisString[] = 
        "Get information about the computer.  The result is a struct holding information about your computer. "
        "Top-level flags in the returned struct are available on all operating systems and identify the operating "
        "operating system: 'macintosh', 'windows', 'osx'.  All other fields in the returned struct are platform-dependent. \n"
        "\n"
        "OS X: results contains a hierarchial struct with major and minor fields names as with BSD's sysctl(3) MIB fields. \n"
        "\n"
        "SCREEN 'Computer' not longer supports the  obsolete usage: \n"
        "[model,owner,system,processor,cache,fpu,hz,busHz,vm,pci,emulating]=SCREEN('Computer')\n";
static char seeAlsoString[] = "";
	 

static void ReportSysctlError(int errorValue)
{
	Boolean	foundError;
    int sysctlErrors[]={EFAULT, EINVAL, ENOMEM, ENOTDIR, EISDIR, EOPNOTSUPP, EPERM};
    int i, errorIndex, numSysctlErrors=7; 
    char *sysctlErrorStrings[]={"EFAULT", "EINVAL", "ENOMEM", "ENOTDIR", "EISDIR", "EOPNOTSUPP", "EPERM", "UNRECOGNIZED"};

    if(errorValue == 0)
            return;
	foundError=0;
	errorIndex=7;
    for(i=0; i<numSysctlErrors; i++){
        if(errno==sysctlErrors[i]){
			foundError=1;
			errorIndex=i;
            break;
		}
    }
    PsychErrorExitMsg(PsychError_internal, sysctlErrorStrings[errorIndex]);
}


PsychError SCREENComputer(void) 
{
    const char *majorStructFieldNames[]={"macintosh", "windows", "osx" ,"linux", "kern", "hw", "processUserLongName", 
	                                     "processUserShortName", "consoleUserName", "machineName", "localHostName", "location", "MACAddress", "system" };
    const char *kernStructFieldNames[]={"ostype", "osrelease", "osrevision", "version","hostname"};
    const char *hwStructFieldNames[]={"machine", "model", "ncpu", "physmem", "usermem", "busfreq", "cpufreq"};
    int numMajorStructDimensions=1, numKernStructDimensions=1, numHwStructDimensions=1;
    int numMajorStructFieldNames=14, numKernStructFieldNames=5, numHwStructFieldNames=7;
    PsychGenericScriptType	*kernStruct, *hwStruct, *majorStruct;
    //char tempStr[CTL_MAXNAME];   //this seems like a bug in Darwin, CTL_MAXNAME is shorter than the longest name.  
    char						tempStr[256], *ethernetMACStr;
    size_t						tempIntSize,  tempStrSize, tempULongIntSize; 	
    unsigned int				mib[2];
	int							tempInt;
	unsigned long int			tempULongInt;
	char						*tempStrPtr;
	CFStringRef					tempCFStringRef;
	Boolean						stringSuccess;
	int							stringLengthChars, ethernetMACStrSizeBytes;
	long						gestaltResult;
	OSErr						gestaltError;
	Str255						systemVersionStr, systemVersionStrForward;
	int							i,strIndex, bcdDigit, lengthSystemVersionString;
    
    //all subfunctions should have these two lines
    PsychPushHelp(useString, synopsisString, seeAlsoString);
    if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};

    PsychErrorExit(PsychCapNumOutputArgs(1));
    PsychErrorExit(PsychCapNumInputArgs(0));
    

    
    //fill the major struct 
    PsychAllocOutStructArray(1, FALSE, numMajorStructDimensions, numMajorStructFieldNames, majorStructFieldNames, &majorStruct);
    PsychSetStructArrayDoubleElement("macintosh", 0, 0, majorStruct);
    PsychSetStructArrayDoubleElement("windows", 0, 0, majorStruct);
    PsychSetStructArrayDoubleElement("linux", 0, 0, majorStruct);
    PsychSetStructArrayDoubleElement("osx", 0, 1, majorStruct);
    
    //fill the kern struct and implant it within the major struct
    PsychAllocOutStructArray(-1, FALSE, numKernStructDimensions, numKernStructFieldNames, kernStructFieldNames, &kernStruct);
    mib[0]=CTL_KERN;
    
    mib[1]=KERN_OSTYPE;
    tempStrSize=sizeof(tempStr);
    ReportSysctlError(sysctl(mib, 2, tempStr, &tempStrSize, NULL, 0));
    PsychSetStructArrayStringElement("ostype", 0, tempStr, kernStruct);
    
    mib[1]=KERN_OSRELEASE;
    tempStrSize=sizeof(tempStr);
    ReportSysctlError(sysctl(mib, 2, tempStr, &tempStrSize, NULL, 0));
    PsychSetStructArrayStringElement("osrelease", 0, tempStr, kernStruct);
    
    mib[1]=KERN_OSREV;
    tempIntSize=sizeof(tempInt);
    ReportSysctlError(sysctl(mib, 2, &tempInt, &tempIntSize, NULL, 0));
    PsychSetStructArrayDoubleElement("osrevision", 0, (double)tempInt, kernStruct);
    
    mib[1]=KERN_VERSION;
    tempStrSize=sizeof(tempStr);
    ReportSysctlError(sysctl(mib, 2, tempStr, &tempStrSize, NULL, 0));
    PsychSetStructArrayStringElement("version", 0, tempStr, kernStruct);
        
    mib[1]=KERN_HOSTNAME;
    tempStrSize=sizeof(tempStr);
    ReportSysctlError(sysctl(mib, 2, tempStr, &tempStrSize, NULL, 0));
    PsychSetStructArrayStringElement("hostname", 0, tempStr, kernStruct);

    PsychSetStructArrayStructElement("kern",0, kernStruct, majorStruct);

    //fill the hw struct and implant it within the major struct
    PsychAllocOutStructArray(-1, FALSE, numHwStructDimensions, numHwStructFieldNames, hwStructFieldNames, &hwStruct);
    mib[0]=CTL_HW;
    
    mib[1]=HW_MACHINE;
    tempStrSize=sizeof(tempStr);
    ReportSysctlError(sysctl(mib, 2, tempStr, &tempStrSize, NULL, 0));
    PsychSetStructArrayStringElement("machine", 0, tempStr, hwStruct);

    mib[1]=HW_MODEL;
    tempStrSize=sizeof(tempStr);
    ReportSysctlError(sysctl(mib, 2, tempStr, &tempStrSize, NULL, 0));
    PsychSetStructArrayStringElement("model", 0, tempStr, hwStruct);
    
    mib[1]=HW_NCPU;
    tempIntSize=sizeof(tempInt);
    ReportSysctlError(sysctl(mib, 2, &tempInt, &tempIntSize, NULL, 0));
    PsychSetStructArrayDoubleElement("ncpu", 0, (double)tempInt, hwStruct);

    mib[1]=HW_MEMSIZE;
    long long tempLongInt;
    tempULongIntSize=sizeof(tempLongInt);
    ReportSysctlError(sysctl(mib, 2, &tempLongInt, &tempULongIntSize, NULL, 0));
    PsychSetStructArrayDoubleElement("physmem", 0, (double)tempLongInt, hwStruct);
    
    mib[1]=HW_USERMEM;
    tempULongIntSize=sizeof(tempULongInt);
    ReportSysctlError(sysctl(mib, 2, &tempULongInt, &tempULongIntSize, NULL, 0));
    PsychSetStructArrayDoubleElement("usermem", 0, (double)tempULongInt, hwStruct);
    
    mib[1]=HW_BUS_FREQ;
    tempULongIntSize=sizeof(tempULongInt);
    ReportSysctlError(sysctl(mib, 2, &tempULongInt, &tempULongIntSize, NULL, 0));
    PsychSetStructArrayDoubleElement("busfreq", 0, (double)tempULongInt, hwStruct);
    
    mib[1]=HW_CPU_FREQ;
    tempULongIntSize=sizeof(tempULongInt);
    ReportSysctlError(sysctl(mib, 2, &tempULongInt, &tempULongIntSize, NULL, 0));
    PsychSetStructArrayDoubleElement("cpufreq", 0, (double)tempULongInt, hwStruct);

    PsychSetStructArrayStructElement("hw",0, hwStruct, majorStruct);

    //fill in the process user, console user and machine name in the root struct.
	tempCFStringRef= CSCopyMachineName();
	stringLengthChars=(int)CFStringGetLength(tempCFStringRef);
	tempStrPtr=malloc(sizeof(char) * (stringLengthChars+1));
	stringSuccess= CFStringGetCString(tempCFStringRef, tempStrPtr, stringLengthChars+1, kCFStringEncodingUTF8);
	PsychSetStructArrayStringElement("machineName", 0, tempStrPtr, majorStruct);
	free(tempStrPtr);
	CFRelease(tempCFStringRef);
	if(!stringSuccess)
		PsychErrorExitMsg(PsychError_internal, "Failed to convert the \"machineName\" field CFString into a C String");
		
	
	tempCFStringRef= CSCopyUserName(TRUE); //use short name
	stringLengthChars=(int)CFStringGetLength(tempCFStringRef);
	tempStrPtr=malloc(sizeof(char) * (stringLengthChars+1));
	stringSuccess= CFStringGetCString(tempCFStringRef, tempStrPtr, stringLengthChars+1, kCFStringEncodingUTF8);
	PsychSetStructArrayStringElement("processUserShortName", 0, tempStrPtr, majorStruct);
	free(tempStrPtr);
	CFRelease(tempCFStringRef);
	if(!stringSuccess)
		PsychErrorExitMsg(PsychError_internal, "Failed to convert the \"processUserShortName\" field CFString into a C String");
	
	tempCFStringRef= CSCopyUserName(FALSE); //use long name
	stringLengthChars=(int)CFStringGetLength(tempCFStringRef);
	tempStrPtr=malloc(sizeof(char) * (stringLengthChars+1));
	stringSuccess= CFStringGetCString(tempCFStringRef, tempStrPtr, stringLengthChars+1, kCFStringEncodingUTF8);
	PsychSetStructArrayStringElement("processUserLongName", 0, tempStrPtr, majorStruct);
	free(tempStrPtr);
	CFRelease(tempCFStringRef);
	if(!stringSuccess)
		PsychErrorExitMsg(PsychError_internal, "Failed to convert the \"processUserLongName\" field CFString into a C String");

	tempCFStringRef= SCDynamicStoreCopyConsoleUser(NULL, NULL, NULL);
	stringLengthChars=(int)CFStringGetLength(tempCFStringRef);
	tempStrPtr=malloc(sizeof(char) * (stringLengthChars+1));
	stringSuccess= CFStringGetCString(tempCFStringRef, tempStrPtr, stringLengthChars+1, kCFStringEncodingUTF8);
    PsychSetStructArrayStringElement("consoleUserName", 0, tempStrPtr, majorStruct);
	free(tempStrPtr);
	CFRelease(tempCFStringRef);
	if(!stringSuccess)
		PsychErrorExitMsg(PsychError_internal, "Failed to convert the \"consoleUserName\" field CFString into a C String");

	tempCFStringRef= SCDynamicStoreCopyLocalHostName (NULL); 
	stringLengthChars=(int)CFStringGetLength(tempCFStringRef);
	tempStrPtr=malloc(sizeof(char) * (stringLengthChars+1));
	stringSuccess= CFStringGetCString(tempCFStringRef, tempStrPtr, stringLengthChars+1, kCFStringEncodingUTF8);
    PsychSetStructArrayStringElement("localHostName", 0, tempStrPtr, majorStruct);
	free(tempStrPtr);
	CFRelease(tempCFStringRef);
	if(!stringSuccess)
		PsychErrorExitMsg(PsychError_internal, "Failed to convert the \"localHostName\" field CFString into a C String");

	tempCFStringRef= SCDynamicStoreCopyLocation(NULL);
	stringLengthChars=(int)CFStringGetLength(tempCFStringRef);
	tempStrPtr=malloc(sizeof(char) * (stringLengthChars+1));
	stringSuccess= CFStringGetCString(tempCFStringRef, tempStrPtr, stringLengthChars+1, kCFStringEncodingUTF8);
    PsychSetStructArrayStringElement("location", 0, tempStrPtr, majorStruct);
	free(tempStrPtr);
	CFRelease(tempCFStringRef);
	if(!stringSuccess)
		PsychErrorExitMsg(PsychError_internal, "Failed to convert the \"location\" field CFString into a C String");
		
	//Add the ethernet MAC address of the primary ethernet interface to the stuct.  This can serve as a unique identifier for the computer.  
    ethernetMACStrSizeBytes=GetPrimaryEthernetAddressStringLengthBytes(TRUE)+1;
	ethernetMACStr=(char*)malloc(sizeof(char) * ethernetMACStrSizeBytes);
	GetPrimaryEthernetAddressString(ethernetMACStr, TRUE, TRUE);
	PsychSetStructArrayStringElement("MACAddress", 0, ethernetMACStr, majorStruct);
	free(ethernetMACStr);
	
	//Add the system version string 
	gestaltError=Gestalt (gestaltSystemVersion, &gestaltResult);
	//The result is a four-digit value stored in BCD in the lower 16-bits  of the result.  There are implicit decimal
	// points between the last three digis.  For example Mac OS 10.3.6 is:
	//
	//  0000 0000 0000 0000 0001 0000 0011 0110
	//                         1    0    3    6
	//                         1    0.   3.   6
	strIndex=0;
	//4th digit.
	bcdDigit=gestaltResult & 15;
	gestaltResult= gestaltResult>>4;
	strIndex=strIndex+sprintf(systemVersionStr+strIndex, "%i", bcdDigit);
	//decimal point
	strIndex=strIndex+sprintf(systemVersionStr+strIndex, "%s", ".");
	//3rd digit
	bcdDigit=gestaltResult & 15;
	gestaltResult= gestaltResult>>4;
	strIndex=strIndex+sprintf(systemVersionStr+strIndex, "%i", bcdDigit);
	//decimal point
	strIndex=strIndex+sprintf(systemVersionStr+strIndex, "%s", ".");
	//second digit
	//2nd digit.
	bcdDigit=gestaltResult & 15;
	gestaltResult= gestaltResult>>4;
	strIndex=strIndex+sprintf(systemVersionStr+strIndex, "%i", bcdDigit);
	//1st digit
	bcdDigit=gestaltResult & 15;
	gestaltResult= gestaltResult>>4;
	strIndex=strIndex+sprintf(systemVersionStr+strIndex, "%i", bcdDigit);
	//preface with "Mac OS "  
	strIndex=strIndex+sprintf(systemVersionStr+strIndex, "%s", " SO caM");
	//reverse to make it forward
	lengthSystemVersionString=strlen(systemVersionStr);
	for(i=0;i<lengthSystemVersionString;i++){
		systemVersionStrForward[lengthSystemVersionString-1-i]=systemVersionStr[i];
	}
	systemVersionStrForward[lengthSystemVersionString]='\0';
	//embed it in the return struct
	PsychSetStructArrayStringElement("system", 0, systemVersionStrForward, majorStruct);

	
    return(PsychError_none);	
}




