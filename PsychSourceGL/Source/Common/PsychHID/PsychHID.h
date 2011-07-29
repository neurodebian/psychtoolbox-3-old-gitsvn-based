	/*
	PsychToolbox3/Source/Common/PsychHID/PsychHID.h		

	PROJECTS: PsychHID

	PLATFORMS:  All

	AUTHORS:

	Allen.Ingling@nyu.edu             awi 
	mario.kleiner@tuebingen.mpg.de    mk
	  
	HISTORY:
		4/29/03  awi		Created.
		3/15/05  dgp        Added Get/SetReport
		3/15/05  dgp        Added missing prototypes from PsychHIDHelpers.c to make compiler happy.
        4/3/05   dgp        Added prototype for PsychHIDReceiveReportsCleanup.
		8/23/07  rpw		Added prototypes for PsychHIDKbTriggerWait and PsychHIDKbQueue suite.
		12/17/09 rpw		Added prototype for PsychHIDGetDeviceListByUsages.

	TO DO:

		DONE	1- Emulate KbCheck
				2- Add Queues
		DONE	3- Have structure returned b GetCollections include list of constituent elements
				4- Try outputs, such as force feedback joysticks
				5- Accept structs as input args as alternative to  device, element or collection indices.
				6- Report vendor-specific usages. 
				7- Return logical arrays
				8- Maintain an internal queue beyond the 50-elements stored by the kernel 
				9- KbCheck could read any device with elements of type button.
        


*/

//begin include once 
#ifndef PSYCH_IS_INCLUDED_PsychHID
#define PSYCH_IS_INCLUDED_PsychHID

//project includes
#include "Psych.h" 
#include "PsychTimeGlue.h"

#define MAXREPORTSIZE 64
#define MAXDEVICEINDEXS 64
#define MAXREPORTS 10000

// OS/X specific includes:
#if PSYCH_SYSTEM == PSYCH_OSX

// Includes for Apple's HID utilities which are layered above the HID device interface.  These are taken from 
// Apple's "HID Explorer" example and used for all generic USB-HID interface support:
#include <Carbon/Carbon.h>
#include <IOKit/HID/IOHIDKeys.h>
#include "HID_Utilities_External.h"

// Required for generic USB device support:
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <mach/mach.h>
#include <string.h>

#else
// Non OS/X:

// Master include file for libusb, as used on Linux and Windows:
#include <libusb.h>

// Master include file for HIDAPI, as used on Linux and Windows:
#include "hidapi.h"

// Dummy-Define so recDevice struct def works:
typedef int recElement;

struct recDevice
{
    void * interface;						// interface to device, NULL = no interface
    void * queue;						// device queue, NULL = no queue
    void * queueRunLoopSource;				        // device queue run loop source, NULL == no source
    void * transaction;					        // output transaction interface, NULL == no interface
    void * notification;					// notifications
    char transport[256];					// device transport (c string)
    long vendorID;						// id for device vendor, unique across all devices
    long productID;						// id for particular product, unique across all of a vendors devices
    long version;						// version of product
    char manufacturer[256];					// name of manufacturer
    char product[256];						// name of product
    char serial[256];						// serial number of specific product, can be assumed unique across specific product or specific vendor (not used often)
    long locID;							// long representing location in USB (or other I/O) chain which device is pluged into, can identify specific device on machine
    long usage;							// usage page which defines general usage
    long usagePage;						// usage within above page which defines specific usage
    long interfaceId;                                           // HIDAPI only: USB interface id.
    long totalElements;						// number of total elements (should be total of all elements on device including collections) (calculated, not reported by device)
    long features;						// number of elements of type kIOHIDElementTypeFeature
    long inputs;						// number of elements of type kIOHIDElementTypeInput_Misc or kIOHIDElementTypeInput_Button or kIOHIDElementTypeInput_Axis or kIOHIDElementTypeInput_ScanCodes
    long outputs;						// number of elements of type kIOHIDElementTypeOutput
    long collections;						// number of elements of type kIOHIDElementTypeCollection
    long axis;							// number of axis (calculated, not reported by device)
    long buttons;						// number of buttons (calculated, not reported by device)
    long hats;							// number of hat switches (calculated, not reported by device)
    long sliders;						// number of sliders (calculated, not reported by device)
    long dials;							// number of dials (calculated, not reported by device)
    long wheels;						// number of wheels (calculated, not reported by device)
    recElement* pListElements; 				        // head of linked list of elements 
    struct recDevice* pNext; 				        // next device
};

typedef struct recDevice recDevice;
typedef recDevice* pRecDevice;

// Internal helpers:
psych_uint32 HIDCountDevices(void);
pRecDevice   HIDGetFirstDevice(void);
pRecDevice   HIDGetNextDevice(pRecDevice pDevice);

#endif

// Structure which carries all required setup and matching parameters for
// finding, opening and configuring a generic USB device. This is passed
// to PsychHIDOSOpenUSBDevice(); to define what device should be opened,
// and with what parameters. It can be easily extended in the future:
struct PsychUSBSetupSpec_Struct {
	int vendorID;			// USB vendor ID.
	int deviceID;			// USB device ID / product ID.
	// More specs go here...
	int configurationID;	// Index of USB configuration to select when configuring the device.
};

typedef struct PsychUSBSetupSpec_Struct PsychUSBSetupSpec;

// Structure to keep track of a generic USB device:
struct PsychUSBDeviceRecord_Struct {
    int    valid;    // 0 = Unused/Free device record, 1 = Active device record.

	// OS-Specific parts of the struct:
	
	#if PSYCH_SYSTEM == PSYCH_OSX
	// OS/X stuff:
	IOUSBDeviceInterface**    device;  // Actual low-level device specific pointer for OS/X.
	#else
        // Linux & Windows stuff:
        libusb_device_handle*     device;  // libusb device handle for other os'es.
	#endif
};

typedef struct PsychUSBDeviceRecord_Struct PsychUSBDeviceRecord;

// Define constants for use by PsychHID files. 
#define PSYCH_HID_MAX_DEVICES								256
#define PSYCH_HID_MAX_DEVICE_ELEMENT_TYPE_NAME_LENGTH 		256
#define PSYCH_HID_MAX_DEVICE_ELEMENT_USAGE_NAME_LENGTH		256
#define PSYCH_HID_MAX_DEVICE_ELEMENTS						1024
#define PSYCH_HID_MAX_KEYBOARD_DEVICES						64
#define PSYCH_HID_MAX_GENERIC_USB_DEVICES					64

// Function prototypes for module subfunctions.
PsychError MODULEVersion(void);						// MODULEVersion.c 
PsychError PSYCHHIDGetNumDevices(void);				// PSYCHHIDGetNumDevices.c
PsychError PSYCHHIDGetDevices(void);				// PsychHIDGetDeviceList.c 

#if PSYCH_SYSTEM == PSYCH_OSX
PsychError PSYCHHIDGetNumElements(void);			// PSYCHHIDGetNumElements.c
PsychError PSYCHHIDGetNumCollections(void);			// PSYCHHIDGetNumCollections.c
PsychError PSYCHHIDGetElements(void);				// PsychHIDGetElementList.c
PsychError PSYCHHIDGetRawState(void);				// PsychHIDGetRawElementState.c
PsychError PSYCHHIDGetCalibratedState(void);		// PsychHIDGetCalibratedState.c
PsychError PSYCHHIDGetCollections(void);			// PsychHIDGetCollections.c
PsychError PSYCHHIDKbWait(void);					// PsychHIDKbWait.c 
PsychError PSYCHHIDKbTriggerWait(void);				// PsychTriggerWait.c
PsychError PSYCHHIDKbQueueCreate(void);				// PsychHIDKbQueueCreate.c
PsychError PSYCHHIDKbQueueStart(void);				// PsychHIDKbQueueStart.c
PsychError PSYCHHIDKbQueueStop(void);				// PsychHIDKbQueueStop.c
PsychError PSYCHHIDKbQueueCheck(void);				// PsychHIDKbQueueCheck.c
PsychError PSYCHHIDKbQueueFlush(void);				// PsychHIDKbQueueFlush.c
PsychError PSYCHHIDKbQueueRelease(void);			// PsychHIDKbQueueRelease.c
#endif

PsychError PSYCHHIDKbCheck(void);					// PsychHIDKbCheck.c

PsychError PSYCHHIDGetReport(void);					// PsychHIDGetReport.c
PsychError PSYCHHIDSetReport(void);					// PsychHIDSetReport.c
PsychError PSYCHHIDReceiveReports(void);			// PsychHIDReceiveReports.c
PsychError PSYCHHIDReceiveReportsStop(void);		// PsychHIDReceiveReportsStop.c
PsychError PSYCHHIDGiveMeReports(void);				// PsychHIDGiveMeReports.c
PsychError PSYCHHIDOpenUSBDevice(void);				// PSYCHHIDOpenUSBDevice.c
PsychError PSYCHHIDCloseUSBDevice(void);			// PSYCHHIDCloseUSBDevice.c
PsychError PSYCHHIDUSBControlTransfer(void);		// PSYCHHIDUSBControlTransfer.c

//internal function protototypes
PsychError  PsychHIDReceiveReportsCleanup(void); // PsychHIDReceiveReports.c
PsychError  ReceiveReports(int deviceIndex); // PsychHIDReceiveReports.c
PsychError  GiveMeReport(int deviceIndex, psych_bool *reportAvailablePtr, unsigned char *reportBuffer, psych_uint32 *reportBytesPtr, double *reportTimePtr); // PsychHIDReceiveReports.c
PsychError  GiveMeReports(int deviceIndex,int reportBytes); // PsychHIDReceiveReports.c
PsychError	ReceiveReportsStop(int deviceIndex);
PsychError 	PsychHIDCleanup(void);												// PsychHIDHelpers.c 
void 		PsychHIDVerifyInit(void);											// PsychHIDHelpers.c 
psych_bool	PsychHIDWarnInputDisabled(const char* callerName);					// PsychHIDHelpers.c
pRecDevice 	PsychHIDGetDeviceRecordPtrFromIndex(int deviceIndex);								// PsychHIDHelpers.c 

#if PSYCH_SYSTEM == PSYCH_OSX
int 		PsychHIDGetIndexFromRecord(pRecDevice deviceRecord, pRecElement elementRecord, HIDElementTypeMask typeMask);	// PsychHIDHelpers.c 
//psych_bool 	PsychHIDCheckOpenDeviceInterfaceFromDeviceIndex(int deviceIndex);						// PsychHIDHelpers.c 
//psych_bool 	PsychHIDCheckOpenDeviceInterfaceFromDeviceRecordPtr(pRecDevice deviceRecord);					// PsychHIDHelpers.c 
void 		PsychHIDVerifyOpenDeviceInterfaceFromDeviceRecordPtr(pRecDevice deviceRecord);					// PsychHIDHelpers.c 
void 		PsychHIDGetTypeMaskStringFromTypeMask(HIDElementTypeMask maskValue, char **pStr);				// PsychHIDHelpers.c
pRecElement 	PsychHIDGetElementRecordFromDeviceRecordAndElementIndex(pRecDevice deviceRecord, int elementIndex);		// PsychHIDHelpers.c
pRecElement 	PsychHIDGetCollectionRecordFromDeviceRecordAndCollectionIndex(pRecDevice deviceRecord, int elementIndex);	// PsychHIDHelpers.c
int 		PsychHIDCountCollectionElements(pRecElement collectionRecord, HIDElementTypeMask elementTypeMask);		// PsychHIDHelpers.c
int 		PsychHIDFindCollectionElements(pRecElement collectionRecord, HIDElementTypeMask elementTypeMask, pRecElement *collectionMembers, int maxListElements);  // PsychHIDHelpers.c
void 		PsychHIDGetDeviceListByUsage(long usagePage, long usage, int *numDeviceIndices, int *deviceIndices, pRecDevice *deviceRecords);  //// PsychHIDHelpers.c
void 		PsychHIDGetDeviceListByUsages(int numUsages, long *usagePages, long *usages, int *numDeviceIndices, int *deviceIndices, pRecDevice *deviceRecords);  //// PsychHIDHelpers.c
psych_bool PsychHIDQueryOpenDeviceInterfaceFromDeviceIndex(int deviceIndex);	// PsychHIDHelpers.c
psych_bool PsychHIDQueryOpenDeviceInterfaceFromDeviceRecordPtr(pRecDevice deviceRecord);	// PsychHIDHelpers.c
// void PsychHIDVerifyOpenDeviceInterfaceFromDeviceIndex(int deviceIndex);	// PsychHIDHelpers.c
#endif

void InitializeSynopsis();
PsychError PsychDisplayPsychHIDSynopsis(void);
int PsychHIDErrors(void* device, int error,char **namePtr,char **descriptionPtr);
void PsychInitializePsychHID(void); // PsychHIDHelpers.c
void PsychHIDCloseAllUSBDevices(void);
PsychUSBDeviceRecord* PsychHIDGetFreeUSBDeviceSlot(int* usbHandle);
PsychUSBDeviceRecord* PsychHIDGetUSBDevice(int usbHandle);

// These must be defined for each OS in their own PsychHIDGenericUSBSupport.c.
psych_bool PsychHIDOSOpenUSBDevice(PsychUSBDeviceRecord* devRecord, int* errorcode, PsychUSBSetupSpec* spec);
void PsychHIDOSCloseUSBDevice(PsychUSBDeviceRecord* devRecord);
int PsychHIDOSControlTransfer(PsychUSBDeviceRecord* devRecord, psych_uint8 bmRequestType, psych_uint8 bRequest, psych_uint16 wValue, psych_uint16 wIndex, psych_uint16 wLength, void *pData);

// These must be defined for each OS in their own PsychHIDStandardInterfaces.c:
void PsychHIDInitializeHIDStandardInterfaces(void);
void PsychHIDShutdownHIDStandardInterfaces(void);
PsychError PsychHIDEnumerateHIDInputDevices(int deviceClass);
PsychError PsychHIDOSKbCheck(int deviceIndex, double* scanList);

//end include once
#endif
