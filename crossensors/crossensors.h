#if !defined(_CROSKEYBOARD_H_)
#define _CROSKEYBOARD_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <initguid.h>
#include <wdm.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)
#include <wdf.h>

#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <hidport.h>

#include "hidcommon.h"
#include "eccmds.h"

extern "C"

NTSTATUS
DriverEntry(
	_In_  PDRIVER_OBJECT   pDriverObject,
	_In_  PUNICODE_STRING  pRegistryPath
	);

EVT_WDF_DRIVER_DEVICE_ADD       OnDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP  OnDriverCleanup;

//
// String definitions
//

#define DRIVERNAME                 "crossensors.sys: "

#define CROSSENSORS_POOL_TAG            (ULONG) 'nsrC'
#define CROSSENSORS_HARDWARE_IDS        L"CoolStar\\CrosSensors\0\0"
#define CROSSENSORS_HARDWARE_IDS_LENGTH sizeof(CROSSENSORS_HARDWARE_IDS)

#define NTDEVICE_NAME_STRING       L"\\Device\\CrosSensors"
#define SYMBOLIC_NAME_STRING       L"\\DosDevices\\CrosSensors"
//
// This is the default report descriptor for the Hid device provided
// by the mini driver in response to IOCTL_HID_GET_REPORT_DESCRIPTOR.
// 

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

//See sensor spec at HidSensorSpec.h
#ifdef DESCRIPTOR_DEF
HID_REPORT_DESCRIPTOR DefaultReportDescriptor[] = {
	0x05, 0x20,                          // USAGE_PAGE (Sensors)
	0x09, 0x01,                          // USAGE (Sensor Collection v2)
	0xa1, 0x00,                          // COLLECTION (Physical)
	0x85, REPORTID_ACCELEROMETER,        //   REPORT_ID (Accelerometer)
	0x05, 0x20,                          //	  USAGE_PAGE (Sensors)
	0x09, 0x73,                          //   USAGE (Sensor Type Accelerometer 3D)
	0xa1, 0x00,                          //   COLLECTION (Physical)
	0x05, 0x20,                          //	    USAGE_PAGE (Sensors)
	0x0a, 0x09, 0x03,                    //		USAGE (Sensor Connection Type)
	0x15, 0x00,                          //		LOGICAL_MINIMUM (0)
	0x25, 0x02,							 //		LOGICAL_MAXIMUM (2)
	0x75, 0x08,                          //		REPORT_SIZE  (8)   - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0xa1, 0x02,                          //		COLLECTION (Logical)
	0x0a, 0x30, 0x08,					 //			USAGE (Sensor Property Connection Type PC Integrated)
	0x0a, 0x31, 0x08,					 //			USAGE (Sensor Property Connection Type PC Attached)
	0x0a, 0x32, 0x08,					 //			USAGE (Sensor Property Connection Type PC External)
	0xb1, 0x00,                          //			FEATURE (Data,Arr,Abs)
	0xc0,                                //		END_COLLECTION

	0x0a, 0x16, 0x03,                    //		USAGE (Sensor Property Reporting State)
	0x15, 0x00,                          //		LOGICAL_MINIMUM (0)
	0x25, 0x02,							 //		LOGICAL_MAXIMUM (5)
	0x75, 0x08,                          //		REPORT_SIZE  (8)   - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0xa1, 0x02,                          //		COLLECTION (Logical)
	0x0a, 0x40, 0x08,					 //			USAGE (Sensor Property Reporting State No Events)
	0x0a, 0x41, 0x08,					 //			USAGE (Sensor Property Reporting State All Events)
	0x0a, 0x42, 0x08,					 //			USAGE (Sensor Property Reporting State Threshold Events)
	0x0a, 0x43, 0x08,					 //			USAGE (Sensor Property Reporting State No Events Wake)
	0x0a, 0x44, 0x08,					 //			USAGE (Sensor Property Reporting State All Events Wake)
	0x0a, 0x45, 0x08,					 //			USAGE (Sensor Property Reporting State Threshold Events Wake)
	0xb1, 0x00,                          //			FEATURE (Data,Arr,Abs)
	0xc0,                                //		END_COLLECTION

	0x0A, 0x01, 0x02,                    //		USAGE (Sensor State)
	0x15, 0x00,                          //		LOGICAL_MINIMUM (0)
	0x25, 0x06,							 //		LOGICAL_MAXIMUM (6)
	0x75, 0x08,                          //		REPORT_SIZE  (8)   - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0xa1, 0x02,                          //		COLLECTION (Logical)
	0x0a, 0x00, 0x08,					 //			USAGE (Sensor State Unknown)
	0x0a, 0x01, 0x08,					 //			USAGE (Sensor State Ready)
	0x0a, 0x02, 0x08,					 //			USAGE (Sensor State Not Available)
	0x0a, 0x03, 0x08,					 //			USAGE (Sensor State No Data)
	0x0a, 0x04, 0x08,					 //			USAGE (Sensor State Initializing)
	0x0a, 0x05, 0x08,					 //			USAGE (Sensor State Access Denied)
	0x0a, 0x06, 0x08,					 //			USAGE (Sensor State Error)
	0xb1, 0x00,                          //			FEATURE (Data,Arr,Abs)
	0xc0,                                //		END_COLLECTION

	0x0a, 0x0e, 0x03,					 //		USAGE (Sensor Property Report Interval)
	0x15, 0x00,                          //		LOGICAL_MINIMUM (0)
	0x26, 0xFF, 0xFF,					 //		LOGICAL_MAXIMUM (-1)
	0x75, 0x10,                          //		REPORT_SIZE  (16) - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0x55, 0x00,							 //		UNIT_EXPONENT (-3)
	0xb1, 0x02,                          //		FEATURE (Data,Var,Abs)

	0x55, 0x00,							 //		UNIT_EXPONENT (0)

	0x0A, 0x52, 0x14,                    //		USAGE (Sensor Data Mod Change Sensivity Abs)
	0x15, 0x00,							 //		LOGICAL_MINIMUM (0)
	0x26, 0xff, 0xff,                    //		LOGICAL_MAXIMUM (65535)
	0x75, 0x10,                          //		REPORT_SIZE  (16) - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0x0A, 0x5B, 0x09,                    //		UNIT (Generic Unit G)
	0x55, 0x0C,                          //		UNIT Exponent (-4)
	0xb1, 0x02,                          //		FEATURE (Data,Var,Abs)

	0x0A, 0x52, 0x24,                    //		USAGE (Sensor Data Mod Max)
	0x16, 0x01, 0x80,                    //		LOGICAL_MINIMUM (-32767)
	0x26, 0xff, 0x7f,                    //		LOGICAL_MAXIMUM (32767)
	0x75, 0x10,                          //		REPORT_SIZE  (16) - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0x0A, 0x5B, 0x09,                    //		UNIT (Generic Unit G)
	0x55, 0x0C,                          //		UNIT Exponent (-4)
	0xb1, 0x02,                          //		FEATURE (Data,Var,Abs)

	0x0A, 0x52, 0x34,                    //		USAGE (Sensor Data Mod Min)
	0x16, 0x01, 0x80,                    //		LOGICAL_MINIMUM (-32767)
	0x26, 0xff, 0x7f,                    //		LOGICAL_MAXIMUM (32767)
	0x75, 0x10,                          //		REPORT_SIZE  (16) - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0x0A, 0x5B, 0x09,                    //		UNIT (Generic Unit G)
	0x55, 0x0C,                          //		UNIT Exponent (-4)
	0xb1, 0x02,                          //		FEATURE (Data,Var,Abs)

	0x05, 0x20,                          //		USAGE_PAGE (0x20 - Sensors)

	0x0A, 0x01, 0x02,                    //		USAGE (Sensor State)
	0x55, 0x00,							 //		UNIT_EXPONENT (0)
	0x15, 0x00,                          //		LOGICAL_MINIMUM (0)
	0x25, 0x06,							 //		LOGICAL_MAXIMUM (6)
	0x75, 0x08,                          //		REPORT_SIZE  (8)   - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0xa1, 0x02,                          //		COLLECTION (Logical)
	0x0a, 0x00, 0x08,					 //			USAGE (Sensor State Unknown)
	0x0a, 0x01, 0x08,					 //			USAGE (Sensor State Ready)
	0x0a, 0x02, 0x08,					 //			USAGE (Sensor State Not Available)
	0x0a, 0x03, 0x08,					 //			USAGE (Sensor State No Data)
	0x0a, 0x04, 0x08,					 //			USAGE (Sensor State Initializing)
	0x0a, 0x05, 0x08,					 //			USAGE (Sensor State Access Denied)
	0x0a, 0x06, 0x08,					 //			USAGE (Sensor State Error)
	0x81, 0x02,                          //			INPUT (Data,Var,Abs)
	0xc0,                                //		END_COLLECTION

	0x0A, 0x02, 0x02,                    //		USAGE (Sensor Event)
	0x15, 0x00,                          //		LOGICAL_MINIMUM (0)
	0x25, 0x05,							 //		LOGICAL_MAXIMUM (5)
	0x75, 0x08,                          //		REPORT_SIZE  (8)   - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0xa1, 0x02,                          //		COLLECTION (Logical)
	0x0a, 0x10, 0x08,					 //			USAGE (Sensor Event Unknown)
	0x0a, 0x11, 0x08,					 //			USAGE (Sensor Event State Changed)
	0x0a, 0x12, 0x08,					 //			USAGE (Sensor Event Property Changed)
	0x0a, 0x13, 0x08,					 //			USAGE (Sensor Event Data Updated)
	0x0a, 0x14, 0x08,					 //			USAGE (Sensor Event Poll Response)
	0x0a, 0x15, 0x08,					 //			USAGE (Sensor Event Change Sensitivity)
	0x81, 0x02,                          //			INPUT (Data,Var,Abs)
	0xc0,                                //		END_COLLECTION

	0x0A, 0x53, 0x04,                    //		USAGE (Acceleration X Axis)
	0x16, 0x01, 0x80,                    //		LOGICAL_MINIMUM (-32767)
	0x26, 0xff, 0x7f,                    //		LOGICAL_MAXIMUM (32767)
	0x75, 0x10,                          //		REPORT_SIZE  (16) - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0x0A, 0x5B, 0x09,                    //		UNIT (Generic Unit G)
	//0x0A, 0x49, 0x09,                    //		UNIT (Generic Unit M/S^2)
	0x55, 0x0C,                          //		UNIT Exponent (-4)
	0x81, 0x02,                          //		INPUT (Data,Var,Abs)

	0x0A, 0x54, 0x04,                    //		USAGE (Acceleration Y Axis)
	0x16, 0x01, 0x80,                    //		LOGICAL_MINIMUM (-32767)
	0x26, 0xff, 0x7f,                    //		LOGICAL_MAXIMUM (32767)
	0x75, 0x10,                          //		REPORT_SIZE  (16) - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0x0A, 0x5B, 0x09,                    //		UNIT (Generic Unit G)
	0x55, 0x0C,                          //		UNIT Exponent (-4)
	0x81, 0x02,                          //		INPUT (Data,Var,Abs)

	0x0A, 0x55, 0x04,                    //		USAGE (Acceleration Z Axis)
	0x16, 0x01, 0x80,                    //		LOGICAL_MINIMUM (-32767)
	0x26, 0xff, 0x7f,                    //		LOGICAL_MAXIMUM (32767)
	0x75, 0x10,                          //		REPORT_SIZE  (16) - bits
	0x95, 0x01,                          //		REPORT_COUNT (1)  - Bytes
	0x0A, 0x5B, 0x09,                    //		UNIT (Generic Unit G)
	0x55, 0x0C,                          //		UNIT Exponent (-4)
	0x81, 0x02,                          //		INPUT (Data,Var,Abs)
	0xc0,                                //   END_COLLECTION
	0xc0,                                // END_COLLECTION
};


//
// This is the default HID descriptor returned by the mini driver
// in response to IOCTL_HID_GET_DEVICE_DESCRIPTOR. The size
// of report descriptor is currently the size of DefaultReportDescriptor.
//

CONST HID_DESCRIPTOR DefaultHidDescriptor = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{ 0x22,   // descriptor type 
	sizeof(DefaultReportDescriptor) }  // total length of report descriptor
};
#endif

#define true 1
#define false 0

typedef struct _CROSEC_COMMAND {
	UINT32 Version;
	UINT32 Command;
	UINT32 OutSize;
	UINT32 InSize;
	UINT32 Result;
	UINT8 Data[];
} CROSEC_COMMAND, * PCROSEC_COMMAND;

typedef
NTSTATUS
(*PCROSEC_CMD_XFER_STATUS)(
	IN      PVOID Context,
	OUT     PCROSEC_COMMAND Msg
	);

typedef
BOOLEAN
(*PCROSEC_CHECK_FEATURES)(
	IN PVOID Context,
	IN INT Feature
	);

typedef
INT
(*PCROSEC_READ_MEM) (
	IN PVOID Context,
	IN INT offset,
	IN INT bytes,
	OUT PVOID dest
	);

typedef enum {
	CSVivaldiRequestUpdateTabletMode = 0x102
} CSVivaldiRequest;

#include <pshpack1.h>
typedef struct CSVivaldiSettingsArg {
	UINT32 argSz;
	CSVivaldiRequest settingsRequest;
	union args {
		struct {
			UINT8 tabletmode;
		} tabletmode;
	} args;
} CSVivaldiSettingsArg, * PCSVivaldiSettingsArg;
#include <poppack.h>

DEFINE_GUID(GUID_CROSEC_INTERFACE_STANDARD_V2,
	0xad8649fa, 0x7c71, 0x11ed, 0xb6, 0x3c, 0x00, 0x15, 0x5d, 0xa4, 0x49, 0xad);

typedef struct _CROSEC_INTERFACE_STANDARD_V2 {
	INTERFACE                        InterfaceHeader;
	PCROSEC_CMD_XFER_STATUS          CmdXferStatus;
	PCROSEC_CHECK_FEATURES           CheckFeatures;
	PCROSEC_READ_MEM                 ReadEcMem;
} CROSEC_INTERFACE_STANDARD_V2, * PCROSEC_INTERFACE_STANDARD_V2;

typedef struct _CROSSENSORS_CONTEXT
{
	WDFDEVICE FxDevice;

	WDFQUEUE ReportQueue;

	WDFQUEUE IdleQueue;

	BYTE TabletMode;
	PCALLBACK_OBJECT CSTabletModeCallback;
	INT callbackUpdateCnt;

	PVOID CrosEcBusContext;

	PCROSEC_CMD_XFER_STATUS CrosEcCmdXferStatus;
	PCROSEC_CHECK_FEATURES  CrosEcCheckFeatures;
	PCROSEC_READ_MEM	    CrosEcReadMem;

	WDFTIMER Timer;
	int SensorCount;
	int LidSensor;

	WDFIOTARGET busIoTarget;

} CROSSENSORS_CONTEXT, *PCROSSENSORS_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CROSSENSORS_CONTEXT, GetDeviceContext)

//
// Power Idle Workitem context
// 
typedef struct _IDLE_WORKITEM_CONTEXT
{
	// Handle to a WDF device object
	WDFDEVICE FxDevice;

	// Handle to a WDF request object
	WDFREQUEST FxRequest;

} IDLE_WORKITEM_CONTEXT, * PIDLE_WORKITEM_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(IDLE_WORKITEM_CONTEXT, GetIdleWorkItemContext)

//
// Function definitions
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_UNLOAD CrosSensorsDriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD CrosSensorsEvtDeviceAdd;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL CrosSensorsEvtInternalDeviceControl;

NTSTATUS
CrosSensorsGetHidDescriptor(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request
	);

NTSTATUS
CrosSensorsGetReportDescriptor(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request
	);

NTSTATUS
CrosSensorsGetDeviceAttributes(
	IN WDFREQUEST Request
	);

NTSTATUS
CrosSensorsGetString(
	IN WDFREQUEST Request
	);

NTSTATUS
CrosSensorsWriteReport(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN WDFREQUEST Request
	);

NTSTATUS
CrosSensorsProcessVendorReport(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN PVOID ReportBuffer,
	IN ULONG ReportBufferLen,
	OUT size_t* BytesWritten
	);

NTSTATUS
CrosSensorsReadReport(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	);

NTSTATUS
CrosSensorsSetFeature(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	);

NTSTATUS
CrosSensorsGetFeature(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	);

PCHAR
DbgHidInternalIoctlString(
	IN ULONG        IoControlCode
	);

VOID
CrosSensorsCompleteIdleIrp(
	IN PCROSSENSORS_CONTEXT FxDeviceContext
);

//
// Helper macros
//

#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#define DBG_INIT  1
#define DBG_PNP   2
#define DBG_IOCTL 4

#if DBG
#define CrosSensorsPrint(dbglevel, dbgcatagory, fmt, ...) {          \
    if (CrosSensorsDebugLevel >= dbglevel &&                         \
        (CrosSensorsDebugCatagories && dbgcatagory))                 \
	    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
	    }                                                           \
}
#else
#define CrosSensorsPrint(dbglevel, fmt, ...) {                       \
}
#endif

#endif
#pragma once
