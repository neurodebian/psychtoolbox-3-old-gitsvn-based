/*
  PsychToolbox/Source/Common/PsychHID/PsychHIDHelpers.c		
  
  PROJECTS: PsychHID
  
  PLATFORMS:  OSX
  
  AUTHORS:
  Allen.Ingling@nyu.edu		awi 
      
  HISTORY:
  5/05/03  awi		Created.
  4/19/05  dgp      cosmetic.
  8/23/07  rpw      added PsychHIDKbQueueRelease() to PsychHIDCleanup()
  4/04/09  mk		added support routines for generic USB devices and usbDeviceRecordBank.

  TO DO:

*/

#include "PsychHID.h"

// Tracker used to maintain references to open generic USB devices.
// PsychUSBDeviceRecord is currently defined in PsychHID.h.
PsychUSBDeviceRecord usbDeviceRecordBank[PSYCH_HID_MAX_GENERIC_USB_DEVICES];

/* PsychInitializePsychHID()
 *
 * Master init routine - Called at module load time / first time init.
 *
 */
void PsychInitializePsychHID(void)
{
	int i;

	// Initialize the generic USB tracker to "all off" state:
	for (i = 0; i < PSYCH_HID_MAX_GENERIC_USB_DEVICES; i++) {
		usbDeviceRecordBank[i].valid = 0;
	}

	return;
}

/* PsychHIDGetFreeUSBDeviceSlot();
 *
 * Return a device record pointer to a free generic USB device
 * slot, as well as the associated numeric usbHandle.
 *
 * Abort with error if no more slots are free.
 */
PsychUSBDeviceRecord* PsychHIDGetFreeUSBDeviceSlot(int* usbHandle)
{
	int i;
	
	// Find the next available USB slot:
	for (i = 0; i < PSYCH_HID_MAX_GENERIC_USB_DEVICES; i++) {
		if (usbDeviceRecordBank[i].valid == 0) {
			*usbHandle = i;
			return( &(usbDeviceRecordBank[i]) );
		}
	}

	// If we reach this point, then all slots are occupied: Fail!
	PsychErrorExitMsg(PsychError_user, "Unable to open another generic USB device! Too many devices open. Please close one and retry.");
	return(NULL);
}

/* PsychHIDGetFreeUSBDeviceSlot();
 *
 * Return a device record pointer to a free generic USB device
 * slot, as well as the associated numeric usbHandle.
 *
 * Abort with error if no more slots are free.
 */
PsychUSBDeviceRecord* PsychHIDGetUSBDevice(int usbHandle)
{
	// Child protection:
	if (usbHandle < 0 || usbHandle >= PSYCH_HID_MAX_GENERIC_USB_DEVICES) PsychErrorExitMsg(PsychError_user, "Invalid generic USB device handle provided! Handle outside valid range.");
	if (usbDeviceRecordBank[usbHandle].valid == 0) PsychErrorExitMsg(PsychError_user, "Invalid generic USB device handle provided! The handle doesn't correspond to an open device.");

	// Valid handle for slot corresponding to an open device. Return PsychUSBDeviceRecord* to it:
	return( &(usbDeviceRecordBank[usbHandle]) );
}

void PsychHIDCloseAllUSBDevices(void)
{
	int i;
	for (i = 0; i < PSYCH_HID_MAX_GENERIC_USB_DEVICES; i++) {
		if (usbDeviceRecordBank[i].valid) {
			PsychHIDOSCloseUSBDevice(PsychHIDGetUSBDevice(i));
		}
	}
}

/*
    PSYCHHIDCheckInit() 
    
    Check to see if we need to create the USB-HID device list. If it has not been created then create it.   
*/
void PsychHIDVerifyInit(void)
{
    if(!HIDHaveDeviceList()) HIDBuildDeviceList( 0, 0);
	PsychHIDWarnInputDisabled(NULL);
}

/*
	PsychHIDWarnInputDisabled()
	
	Check if HID event input is disabled by external processes, e.g., due to
	secure password entry protection. Return TRUE and output a warning message
	to user if HID input won't work due to active security measures. Return FALSE
	and stay silent if PsychHID can work as expected.
	
*/
psych_bool PsychHIDWarnInputDisabled(const char* callerName)
{
	if (IsSecureEventInputEnabled()) {
		printf("PTB-WARNING: During %s: Some other running application is preventing me from accessing the keyboard/keypad/mouse/...!\n", (callerName) ? callerName : "PsychHID invocation");
		printf("PTB-WARNING: This is likely a security measure, e.g., to protect some active password entry field.\n");
		printf("PTB-WARNING: Please identify and quit the offending application. E.g., some versions of Firefox are known to cause such problems...\n");
		return(TRUE);
	}

	return(FALSE);
}

/*
    PsychHIDCleanup() 
    
    Cleanup before flushing the mex file.   
*/
PsychError PsychHIDCleanup(void) 
{
	long error;

	// Disable online help system:
	PsychClearGiveHelp();
	
	// Shutdown keyboard queue functions on OS/X:
	error=PSYCHHIDKbQueueRelease();			// PsychHIDKbQueueRelease.c, but has to be called with uppercase PSYCH because that's how it's registered (otherwise crashes on clear mex)

	// Shutdown USB-HID report low-level functions, e.g., for DAQ toolbox on OS/X:
	error=PsychHIDReceiveReportsCleanup(); // PsychHIDReceiveReport.c
	
	// Release all other HID device data structures:
    if(HIDHaveDeviceList())HIDReleaseDeviceList();
	
	// Close and release all open generic USB devices:
	PsychHIDCloseAllUSBDevices();

    return(PsychError_none);
}

/* 
    PsychHIDGetDeviceRecordPtrFromIndex()
    
    The inverse of PsychHIDGetIndexFromRecord()
    
    Accept the index from the list of device records and return a pointer to the indicated record.  Externally the list is one-indexed.  
*/
pRecDevice PsychHIDGetDeviceRecordPtrFromIndex(int deviceIndex)
{
    int				i;
    pRecDevice 			currentDevice=NULL;

    PsychHIDVerifyInit();
    i=1;
    for(currentDevice=HIDGetFirstDevice(); currentDevice != NULL; currentDevice=HIDGetNextDevice(currentDevice)){    
        if(i==deviceIndex)
            return(currentDevice);
        ++i;
    }
    PsychErrorExitMsg(PsychError_internal, "Invalid device index specified.  Has a device has been unplugged? Try rebuilding the device list");
    return(NULL);  //make the compiler happy.
}


/*
    PsychHIDGetDeviceListByUsage()
    
    
*/ 
void PsychHIDGetDeviceListByUsage(long usagePage, long usage, int *numDeviceIndices, int *deviceIndices, pRecDevice *deviceRecords)
{
    pRecDevice 			currentDevice;
    int				currentDeviceIndex;

    PsychHIDVerifyInit();
    currentDeviceIndex=0;
    *numDeviceIndices=0;
    for(currentDevice=HIDGetFirstDevice(); currentDevice != NULL; currentDevice=HIDGetNextDevice(currentDevice)){    
        ++currentDeviceIndex;     
        if(currentDevice->usagePage==usagePage && currentDevice->usage==usage){
            deviceRecords[*numDeviceIndices]=currentDevice;
            deviceIndices[*numDeviceIndices]=currentDeviceIndex;  //the array is 0-indexed, devices are 1-indexed.   
            ++(*numDeviceIndices);
        }
    }
}
 
/*
 PsychHIDGetDeviceListByUsages()
 
 
 */ 
void PsychHIDGetDeviceListByUsages(int numUsages, long *usagePages, long *usages, int *numDeviceIndices, int *deviceIndices, pRecDevice *deviceRecords)
{
    pRecDevice 			currentDevice;
    int				currentDeviceIndex;
    int				currentUsage;
    long 			*usagePage;
    long			*usage;
	
    PsychHIDVerifyInit();
    *numDeviceIndices=0;
    for(usagePage=usagePages, usage=usages, currentUsage=0; currentUsage<numUsages; usagePage++, usage++, currentUsage++){
		currentDeviceIndex=0;
		for(currentDevice=HIDGetFirstDevice(); currentDevice != NULL; currentDevice=HIDGetNextDevice(currentDevice)){    
			++currentDeviceIndex;     
			if(currentDevice->usagePage==*usagePage && currentDevice->usage==*usage){
				deviceRecords[*numDeviceIndices]=currentDevice;
				deviceIndices[*numDeviceIndices]=currentDeviceIndex;  //the array is 0-indexed, devices are 1-indexed.   
				++(*numDeviceIndices);
			}
		}
	}
}




/*
    PsychHIDGetIndexFromRecord()
    
    The inverse of PsychHIDGetDeviceRecordPtrFromIndex. 
    
    This O(n) where n is the number of device elements.   We could make it O(1) if we modified
    the element structure in the HID Utilities library to include a field specifying the index of the element or 
    collection.
    
    Note that if PsychHIDGetIndexFromRecord() is O(n) then its caller, PsychHIDGetCollections, is O(n^2) for each
    device, whereas if PsychHIDGetIndexFromRecord() is O(1) then psychHIDGetCollections becomes O(n) for each 
    device.   
*/
int PsychHIDGetIndexFromRecord(pRecDevice deviceRecord, pRecElement elementRecord, HIDElementTypeMask typeMask)
{
    int 		elementIndex;
    pRecElement		currentElement;						
    
    if(elementRecord==NULL)
        return(0);
    elementIndex=1;
    for(currentElement=HIDGetFirstDeviceElement(deviceRecord, typeMask);
        currentElement != elementRecord && currentElement != NULL;
        currentElement=HIDGetNextDeviceElement(currentElement, typeMask))
        ++elementIndex;
    if(currentElement==elementRecord)
        return(elementIndex);
    else{
        PsychErrorExitMsg(PsychError_internal, "Element record not found within device record");
        return(0); //make the compiler happy
    }    
    
}



pRecElement PsychHIDGetElementRecordFromDeviceRecordAndElementIndex(pRecDevice deviceRecord, int elementIndex)
{
    int				i;
    pRecElement			currentElement;

    PsychHIDVerifyInit();
    i=1;
    for(currentElement=HIDGetFirstDeviceElement(deviceRecord, kHIDElementTypeIO); 
        currentElement != NULL; 
        currentElement=HIDGetNextDeviceElement (currentElement, kHIDElementTypeIO))
    {    
        if(i==elementIndex)
            return(currentElement);
        ++i;
    }
    PsychErrorExitMsg(PsychError_internal, "Invalid device index specified.  Has a device has been unplugged? Try rebuilding the device list");
    return(NULL);  //make the compiler happy.

}



pRecElement PsychHIDGetCollectionRecordFromDeviceRecordAndCollectionIndex(pRecDevice deviceRecord, int elementIndex)
{
    int				i;
    pRecElement			currentElement;

    PsychHIDVerifyInit();
    i=1;
    for(currentElement=HIDGetFirstDeviceElement(deviceRecord, kHIDElementTypeCollection); 
        currentElement != NULL; 
        currentElement=HIDGetNextDeviceElement (currentElement, kHIDElementTypeCollection))
    {    
        if(i==elementIndex)
            return(currentElement);
        ++i;
    }
    PsychErrorExitMsg(PsychError_internal, "Invalid collection index specified.  Has a device has been unplugged? Try rebuilding the device list");
    return(NULL);  //make the compiler happy.

}



/*
        PsychHIDQueryOpenDeviceInterfaceFromDeviceIndex()

        Check the interface field of the libHIDUtilities device structure for NULLness.  libHIDUtilities.h seems to indicate that it is neccessary for application
        to invoke HIDCreateOpenDeviceInterface() before accessing a device.  However,
        1) libHIDUtilities provides no way to obtain a value for the required first argument to HIDCreateOpenDeviceInterface().  
        2) Apple's example HID Explorer application does not call HIDCreateOpenDeviceInterface().   
        3) Internally, libHIDUtilities invokes HIDCreateOpenDeviceInterface() itself when HIDBuildDeviceList() is called.
        
        Because the call lies within mysterious conditionals there is some uncertainty about whether HIDCreateOpenDeviceInterface() will always 
        invoke HIDBuildDeviceList().  Therefore, PsychHID verifies that the device interface has been opened before accessing the elements of a device.
        
*/ 
psych_bool PsychHIDQueryOpenDeviceInterfaceFromDeviceIndex(int deviceIndex)
{
    pRecDevice 			deviceRecord;

    PsychHIDVerifyInit();
    deviceRecord=PsychHIDGetDeviceRecordPtrFromIndex(deviceIndex);
    return(deviceRecord->interface != NULL);    
}

psych_bool PsychHIDQueryOpenDeviceInterfaceFromDeviceRecordPtr(pRecDevice deviceRecord)
{
    PsychHIDVerifyInit();
    return(deviceRecord->interface != NULL);
}

void PsychHIDVerifyOpenDeviceInterfaceFromDeviceIndex(int deviceIndex)
{
    if(!PsychHIDQueryOpenDeviceInterfaceFromDeviceIndex(deviceIndex))
        PsychErrorExitMsg(PsychError_internal, "Device interface field is NULL.  libHIDUtilities failed to open the device interface ?");
}

void PsychHIDVerifyOpenDeviceInterfaceFromDeviceRecordPtr(pRecDevice deviceRecord)
{
    if(!PsychHIDQueryOpenDeviceInterfaceFromDeviceRecordPtr(deviceRecord))
        PsychErrorExitMsg(PsychError_internal, "Device interface field is NULL.  libHIDUtilities failed to open the device interface ?");
}


/*
    PsychHIDGetTypeMaskStringFromTypeMask()
    
    Apple's HID Utilities library uses two different specificationos of the device type:
        - enumerated type HIDElementTypeMask, used to specify which element types to return.
        - unsigned long type identifier constants.  
        
    The mask values will not mask the unsigned long type identifiers.  
*/
void PsychHIDGetTypeMaskStringFromTypeMask(HIDElementTypeMask maskValue, char **pStr)
{
    char *maskNames[]={"input", "output", "feature", "collection", "io", "all", };
    
    switch(maskValue)
    {
        case kHIDElementTypeInput:
            *pStr=maskNames[0];
            break;
        case kHIDElementTypeOutput:
            *pStr=maskNames[1];
            break;
        case kHIDElementTypeFeature:
            *pStr=maskNames[2];
            break;
        case kHIDElementTypeCollection:
            *pStr=maskNames[3];
            break;
        case kHIDElementTypeIO:
            *pStr=maskNames[4];
            break;
        case kHIDElementTypeAll:
            *pStr=maskNames[5];
            break;
    }
}



/*
    PsychHIDCountCollectionElements()
    
    Non-recursively count all elements of a collection which are of the specified type.
    
    HID element records hold three pointers to other element records: pPrevious, pChild and pSibling.  PsychHIDCountCollectionElements() 
    operates on the theory that the members of a collection are its child and all of that child's siblings.
    
  
*/
int PsychHIDCountCollectionElements(pRecElement collectionRecord, HIDElementTypeMask elementTypeMask)
{
    pRecElement		currentElement;
    int			numElements=0;
    HIDElementTypeMask	currentElementMaskValue;
    
    for(currentElement=collectionRecord->pChild; currentElement != NULL; currentElement= currentElement->pSibling)
    {
        currentElementMaskValue=HIDConvertElementTypeToMask(currentElement->type);  
        if(currentElementMaskValue & elementTypeMask)
            ++numElements;
    }
    return(numElements);
}




/*
    FindCollectionElements()
    
    Non-recursively return of a list of a collection's memember elements.
    
    HID element records hold three pointers to other element records: pPrevious, pChild and pSibling.  FindCollectionElements() 
    operates on the theory that the members of a collection are its child and all of that child's siblings.
    
*/
int PsychHIDFindCollectionElements(pRecElement collectionRecord, HIDElementTypeMask elementTypeMask, pRecElement *collectionMembers, int maxListElements)
{
    pRecElement		currentElement;
    int			numElements=0;
    HIDElementTypeMask	currentElementMaskValue;
    
    for(currentElement=collectionRecord->pChild; currentElement != NULL; currentElement= currentElement->pSibling)
    {
        currentElementMaskValue=HIDConvertElementTypeToMask(currentElement->type);  
        if(currentElementMaskValue & elementTypeMask){
            if(numElements == maxListElements)
                PsychErrorExitMsg(PsychError_internal, "Number of collection elements exceeds allocated storage space." );
            collectionMembers[numElements]=currentElement;
            ++numElements;
        }
    }
    return(numElements);
}
	
