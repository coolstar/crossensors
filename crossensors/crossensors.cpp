#define DESCRIPTOR_DEF
#include "crossensors.h"
#include <acpiioct.h>
#include <ntstrsafe.h>

static ULONG CrosSensorsDebugLevel = 100;
static ULONG CrosSensorsDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT  DriverObject,
	__in PUNICODE_STRING RegistryPath
	)
{
	NTSTATUS               status = STATUS_SUCCESS;
	WDF_DRIVER_CONFIG      config;
	WDF_OBJECT_ATTRIBUTES  attributes;

	CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Driver Entry");

	WDF_DRIVER_CONFIG_INIT(&config, CrosSensorsEvtDeviceAdd);

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

	//
	// Create a framework driver object to represent our driver.
	//

	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		&attributes,
		&config,
		WDF_NO_HANDLE
		);

	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"WdfDriverCreate failed with status 0x%x\n", status);
	}

	return status;
}

NTSTATUS ConnectToEc(
	_In_ WDFDEVICE FxDevice
) {
	PCROSSENSORS_CONTEXT pDevice = GetDeviceContext(FxDevice);
	WDF_OBJECT_ATTRIBUTES objectAttributes;

	WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
	objectAttributes.ParentObject = FxDevice;

	NTSTATUS status = WdfIoTargetCreate(FxDevice,
		&objectAttributes,
		&pDevice->busIoTarget
	);
	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error creating IoTarget object - 0x%x\n",
			status);
		if (pDevice->busIoTarget)
			WdfObjectDelete(pDevice->busIoTarget);
		return status;
	}

	DECLARE_CONST_UNICODE_STRING(busDosDeviceName, L"\\DosDevices\\GOOG0004");

	WDF_IO_TARGET_OPEN_PARAMS openParams;
	WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
		&openParams,
		&busDosDeviceName,
		(GENERIC_READ | GENERIC_WRITE));

	openParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
	openParams.CreateDisposition = FILE_OPEN;
	openParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;

	CROSEC_INTERFACE_STANDARD_V2 CrosEcInterface;
	RtlZeroMemory(&CrosEcInterface, sizeof(CrosEcInterface));

	status = WdfIoTargetOpen(pDevice->busIoTarget, &openParams);
	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error opening IoTarget object - 0x%x\n",
			status);
		WdfObjectDelete(pDevice->busIoTarget);
		return status;
	}

	status = WdfIoTargetQueryForInterface(pDevice->busIoTarget,
		&GUID_CROSEC_INTERFACE_STANDARD_V2,
		(PINTERFACE)&CrosEcInterface,
		sizeof(CrosEcInterface),
		2,
		NULL);
	WdfIoTargetClose(pDevice->busIoTarget);
	pDevice->busIoTarget = NULL;
	if (!NT_SUCCESS(status)) {
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfFdoQueryForInterface failed 0x%x\n", status);
		return status;
	}

	pDevice->CrosEcBusContext = CrosEcInterface.InterfaceHeader.Context;
	pDevice->CrosEcCmdXferStatus = CrosEcInterface.CmdXferStatus;
	pDevice->CrosEcCheckFeatures = CrosEcInterface.CheckFeatures;
	pDevice->CrosEcReadMem = CrosEcInterface.ReadEcMem;
	return status;
}

static NTSTATUS send_ec_command(
	_In_ PCROSSENSORS_CONTEXT pDevice,
	UINT32 cmd,
	UINT32 version,
	UINT8* out,
	size_t outSize,
	UINT8* in,
	size_t inSize)
{
	PCROSEC_COMMAND msg = (PCROSEC_COMMAND)ExAllocatePoolWithTag(NonPagedPool, sizeof(CROSEC_COMMAND) + max(outSize, inSize), CROSSENSORS_POOL_TAG);
	if (!msg) {
		return STATUS_NO_MEMORY;
	}
	msg->Version = version;
	msg->Command = cmd;
	msg->OutSize = outSize;
	msg->InSize = inSize;

	if (outSize)
		memcpy(msg->Data, out, outSize);

	NTSTATUS status = (*pDevice->CrosEcCmdXferStatus)(pDevice->CrosEcBusContext, msg);
	if (!NT_SUCCESS(status)) {
		goto exit;
	}

	if (in && inSize) {
		memcpy(in, msg->Data, inSize);
	}

exit:
	ExFreePoolWithTag(msg, CROSSENSORS_POOL_TAG);
	return status;
}

/**
 * Get the versions of the command supported by the EC.
 *
 * @param cmd		Command
 * @param pmask		Destination for version mask; will be set to 0 on
 *			error.
 */
static NTSTATUS cros_ec_get_cmd_versions(PCROSSENSORS_CONTEXT pDevice, int cmd, UINT32* pmask) {
	struct ec_params_get_cmd_versions_v1 pver_v1;
	struct ec_params_get_cmd_versions pver;
	struct ec_response_get_cmd_versions rver;
	NTSTATUS status;

	*pmask = 0;

	pver_v1.cmd = cmd;
	status = send_ec_command(pDevice, EC_CMD_GET_CMD_VERSIONS, 1, (UINT8*)&pver_v1, sizeof(pver_v1),
		(UINT8*)&rver, sizeof(rver));

	if (!NT_SUCCESS(status)) {
		pver.cmd = cmd;
		status = send_ec_command(pDevice, EC_CMD_GET_CMD_VERSIONS, 0, (UINT8*)&pver, sizeof(pver),
			(UINT8*)&rver, sizeof(rver));
	}

	*pmask = rver.version_mask;
	return status;
}

/**
 * Return non-zero if the EC supports the command and version
 *
 * @param cmd		Command to check
 * @param ver		Version to check
 * @return non-zero if command version supported; 0 if not.
 */
BOOLEAN cros_ec_cmd_version_supported(PCROSSENSORS_CONTEXT pDevice, int cmd, int ver)
{
	UINT32 mask = 0;

	if (NT_SUCCESS(cros_ec_get_cmd_versions(pDevice, cmd, &mask)))
		return false;

	return (mask & EC_VER_MASK(ver)) ? true : false;
}

static int cros_ec_sensors_cmd_read_u8(PCROSSENSORS_CONTEXT pDevice,
	unsigned int offset, UINT8* dest)
{
	return (*pDevice->CrosEcReadMem)(pDevice->CrosEcBusContext, offset, 1, dest);
}

static int cros_ec_sensors_cmd_read_u16(PCROSSENSORS_CONTEXT pDevice,
	unsigned int offset, UINT16* dest)
{
	UINT16 tmp;
	int ret = (*pDevice->CrosEcReadMem)(pDevice->CrosEcBusContext, offset, 2, &tmp);

	if (ret >= 0)
		*dest = tmp;

	return ret;
}

static int cros_ec_sensors_read_until_not_busy(PCROSSENSORS_CONTEXT pDevice)
{
	UINT8 status;
	int ret, attempts = 0;

	ret = cros_ec_sensors_cmd_read_u8(pDevice, EC_MEMMAP_ACC_STATUS, &status);
	if (ret < 0)
		return ret;

	while (status & EC_MEMMAP_ACC_STATUS_BUSY_BIT) {
		/* Give up after enough attempts, return error. */
		if (attempts++ >= 50)
			return STATUS_IO_TIMEOUT;

		/* Small delay every so often. */
		if (attempts % 5 == 0) {
			LARGE_INTEGER Interval;
			Interval.QuadPart = -10 * 1000 * 25;
			KeDelayExecutionThread(KernelMode, false, &Interval); //msleep(25)
		}

		ret = cros_ec_sensors_cmd_read_u8(pDevice, EC_MEMMAP_ACC_STATUS,
			&status);
		if (ret < 0)
			return ret;
	}

	return status;
}

int read_lpc_sensor(PCROSSENSORS_CONTEXT pDevice, int sensorNum, UINT16 *data) {
	UINT8 samp_id = 0xff, status = 0;
	int ret, attempts = 0;

	/*
	 * Continually read all data from EC until the status byte after
	 * all reads reflects that the EC is not busy and the sample id
	 * matches the sample id from before all reads. This guarantees
	 * that data read in was not modified by the EC while reading.
	 */
	while ((status & (EC_MEMMAP_ACC_STATUS_BUSY_BIT |
		EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK)) != samp_id) {
		/* If we have tried to read too many times, return error. */
		if (attempts++ >= 5)
			return STATUS_IO_TIMEOUT;

		/* Read status byte until EC is not busy. */
		ret = cros_ec_sensors_read_until_not_busy(pDevice);
		if (ret < 0)
			return ret;

		/*
		 * Store the current sample id so that we can compare to the
		 * sample id after reading the data.
		 */
		samp_id = ret & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK;

		/* Read all EC data, format it, and store it into data. */


		ret = pDevice->CrosEcReadMem(pDevice->CrosEcBusContext,
			EC_MEMMAP_ACC_DATA + 2 + (sensorNum * 6), 6,
			data);
		if (ret < 0)
			return ret;

		/* Read status byte. */
		ret = cros_ec_sensors_cmd_read_u8(pDevice, EC_MEMMAP_ACC_STATUS,
			&status);
		if (ret < 0)
			return ret;
	}

	return 0;
}

NTSTATUS
GetDeviceTabletModeState(
	_In_ WDFDEVICE FxDevice
)
{
	NTSTATUS status = STATUS_ACPI_NOT_INITIALIZED;
	ACPI_EVAL_INPUT_BUFFER_EX inputBuffer;
	RtlZeroMemory(&inputBuffer, sizeof(inputBuffer));

	inputBuffer.Signature = ACPI_EVAL_INPUT_BUFFER_SIGNATURE_EX;
	status = RtlStringCchPrintfA(
		inputBuffer.MethodName,
		sizeof(inputBuffer.MethodName),
		"TBMC"
	);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	WDFMEMORY outputMemory;
	PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
	size_t outputArgumentBufferSize = 32;
	size_t outputBufferSize = FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument) + outputArgumentBufferSize;

	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = FxDevice;

	status = WdfMemoryCreate(&attributes,
		NonPagedPoolNx,
		0,
		outputBufferSize,
		&outputMemory,
		(PVOID*)&outputBuffer);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	RtlZeroMemory(outputBuffer, outputBufferSize);

	WDF_MEMORY_DESCRIPTOR inputMemDesc;
	WDF_MEMORY_DESCRIPTOR outputMemDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemDesc, &inputBuffer, (ULONG)sizeof(inputBuffer));
	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&outputMemDesc, outputMemory, NULL);

	status = WdfIoTargetSendInternalIoctlSynchronously(
		WdfDeviceGetIoTarget(FxDevice),
		NULL,
		IOCTL_ACPI_EVAL_METHOD_EX,
		&inputMemDesc,
		&outputMemDesc,
		NULL,
		NULL
	);
	if (!NT_SUCCESS(status)) {
		goto Exit;
	}

	if (outputBuffer->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE) {
		goto Exit;
	}

	if (outputBuffer->Count < 1) {
		goto Exit;
	}

	PCROSSENSORS_CONTEXT pDevice = GetDeviceContext(FxDevice);
	pDevice->TabletMode = (outputBuffer->Argument[0].Data[0] != 0);

Exit:
	if (outputMemory != WDF_NO_HANDLE) {
		WdfObjectDelete(outputMemory);
	}
	return status;
}

void SensorWorkItem(
	IN WDFWORKITEM WorkItem
) {
	WDFDEVICE Device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
	PCROSSENSORS_CONTEXT pDevice = GetDeviceContext(Device);

	if (pDevice->LidSensor != -1) {
		UINT16 data[3] = { 0 };
		NTSTATUS status;

		int ret = read_lpc_sensor(pDevice, pDevice->LidSensor, data);

		CrosSensorsAccelReport report;
		report.ReportID = REPORTID_ACCELEROMETER;
		report.SensorState = SENSOR_STATE_READY_SEL; //Ready
		report.SensorEvent = SENSOR_EVENT_DATA_UPDATED_SEL; //Data Updated
		report.X = data[0] * -1;
		report.Y = data[1] * -1;
		report.Z = data[2] * -1;

		size_t BytesWritten;
		CrosSensorsProcessVendorReport(pDevice, &report, sizeof(report), &BytesWritten);
	}

	pDevice->callbackUpdateCnt++;
	if (pDevice->callbackUpdateCnt > 10) {
		pDevice->callbackUpdateCnt = 0;

		if (NT_SUCCESS(GetDeviceTabletModeState(pDevice->FxDevice))) {
			CSVivaldiSettingsArg newArg;
			RtlZeroMemory(&newArg, sizeof(CSVivaldiSettingsArg));
			newArg.argSz = sizeof(CSVivaldiSettingsArg);
			newArg.settingsRequest = CSVivaldiRequestUpdateTabletMode;
			newArg.args.tabletmode.tabletmode = (UINT8)pDevice->TabletMode;
			ExNotifyCallback(pDevice->CSTabletModeCallback, &newArg, NULL);
		}
	}

	WdfObjectDelete(WorkItem);
}

void SensorTimerFunc(_In_ WDFTIMER hTimer) {
	WDFDEVICE Device = (WDFDEVICE)WdfTimerGetParentObject(hTimer);
	PCROSSENSORS_CONTEXT pDevice = GetDeviceContext(Device);

	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_WORKITEM_CONFIG workitemConfig;
	WDFWORKITEM hWorkItem;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&attributes, CROSSENSORS_CONTEXT);
	attributes.ParentObject = Device;
	WDF_WORKITEM_CONFIG_INIT(&workitemConfig, SensorWorkItem);

	WdfWorkItemCreate(&workitemConfig,
		&attributes,
		&hWorkItem);

	WdfWorkItemEnqueue(hWorkItem);
}

NTSTATUS
OnPrepareHardware(
	_In_  WDFDEVICE     FxDevice,
	_In_  WDFCMRESLIST  FxResourcesRaw,
	_In_  WDFCMRESLIST  FxResourcesTranslated
	)
	/*++

	Routine Description:

	This routine caches the SPB resource connection ID.

	Arguments:

	FxDevice - a handle to the framework device object
	FxResourcesRaw - list of translated hardware resources that
	the PnP manager has assigned to the device
	FxResourcesTranslated - list of raw hardware resources that
	the PnP manager has assigned to the device

	Return Value:

	Status

	--*/
{
	PCROSSENSORS_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(FxResourcesRaw);
	UNREFERENCED_PARAMETER(FxResourcesTranslated);

	status = ConnectToEc(FxDevice);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	(*pDevice->CrosEcCmdXferStatus)(pDevice->CrosEcBusContext, NULL);
	if (!(*pDevice->CrosEcCheckFeatures)(pDevice->CrosEcBusContext, EC_FEATURE_MOTION_SENSE)) {
		return STATUS_DEVICE_NOT_CONNECTED;
	}

	ec_params_motion_sense params = { 0 };
	ec_response_motion_sense resp;

	params.cmd = MOTIONSENSE_CMD_DUMP;

	status = send_ec_command(pDevice, EC_CMD_MOTION_SENSE_CMD, 1, (UINT8*)&params, sizeof(params), (UINT8*)&resp, sizeof(resp));
	if (!NT_SUCCESS(status)) {
		return status;
	}

	pDevice->SensorCount = resp.dump.sensor_count;

	pDevice->LidSensor = -1;
	for (int i = 0; i < pDevice->SensorCount; i++) {
		params = { 0 };
		params.cmd = MOTIONSENSE_CMD_INFO;
		params.info.sensor_num = i;

		status = send_ec_command(pDevice, EC_CMD_MOTION_SENSE_CMD, 1, (UINT8*)&params, sizeof(params), (UINT8*)&resp, sizeof(resp));
		if (!NT_SUCCESS(status)) {
			return status;
		}
		if (resp.info.type == MOTIONSENSE_TYPE_ACCEL &&
			resp.info.location == MOTIONSENSE_LOC_LID) {
			pDevice->LidSensor = i;
		}
	}

	return status;
}

NTSTATUS
OnSelfManagedIoInit(
	_In_
	WDFDEVICE FxDevice
) {
	PCROSSENSORS_CONTEXT pDevice;
	pDevice = GetDeviceContext(FxDevice);

	NTSTATUS status = STATUS_SUCCESS;

	// CS Keyboard Callback

	UNICODE_STRING CSKeyboardSettingsCallbackAPI;
	RtlInitUnicodeString(&CSKeyboardSettingsCallbackAPI, L"\\CallBack\\CsKeyboardSettingsCallbackAPI");

	OBJECT_ATTRIBUTES attributes;
	InitializeObjectAttributes(&attributes,
		&CSKeyboardSettingsCallbackAPI,
		OBJ_KERNEL_HANDLE | OBJ_OPENIF | OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
		NULL,
		NULL
	);
	status = ExCreateCallback(&pDevice->CSTabletModeCallback, &attributes, TRUE, TRUE);
	if (!NT_SUCCESS(status)) {

		return status;
	}

	return status;
}

NTSTATUS
OnReleaseHardware(
	_In_  WDFDEVICE     FxDevice,
	_In_  WDFCMRESLIST  FxResourcesTranslated
	)
	/*++

	Routine Description:

	Arguments:

	FxDevice - a handle to the framework device object
	FxResourcesTranslated - list of raw hardware resources that
	the PnP manager has assigned to the device

	Return Value:

	Status

	--*/
{
	PCROSSENSORS_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(FxResourcesTranslated);

	if (pDevice->CSTabletModeCallback) {
		ObfDereferenceObject(pDevice->CSTabletModeCallback);
		pDevice->CSTabletModeCallback = NULL;
	}


	return status;
}

NTSTATUS
OnD0Entry(
	_In_  WDFDEVICE               FxDevice,
	_In_  WDF_POWER_DEVICE_STATE  FxPreviousState
	)
	/*++

	Routine Description:

	This routine allocates objects needed by the driver.

	Arguments:

	FxDevice - a handle to the framework device object
	FxPreviousState - previous power state

	Return Value:

	Status

	--*/
{
	UNREFERENCED_PARAMETER(FxPreviousState);

	PCROSSENSORS_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	ec_params_motion_sense params = { 0 };
	ec_response_motion_sense resp;

	params.cmd = MOTIONSENSE_CMD_FIFO_INT_ENABLE;
	params.fifo_int_enable.enable = 0;

	status = send_ec_command(pDevice, EC_CMD_MOTION_SENSE_CMD, 1, (UINT8*)&params, sizeof(params), (UINT8*)&resp, sizeof(resp));
	if (!NT_SUCCESS(status)) {
		return status;
	}

	WdfTimerStart(pDevice->Timer, WDF_REL_TIMEOUT_IN_MS(10));

	CrosSensorsCompleteIdleIrp(pDevice);

	return status;
}

NTSTATUS
OnD0Exit(
	_In_  WDFDEVICE               FxDevice,
	_In_  WDF_POWER_DEVICE_STATE  FxPreviousState
	)
	/*++

	Routine Description:

	This routine destroys objects needed by the driver.

	Arguments:

	FxDevice - a handle to the framework device object
	FxPreviousState - previous power state

	Return Value:

	Status

	--*/
{
	UNREFERENCED_PARAMETER(FxPreviousState);

	PCROSSENSORS_CONTEXT pDevice = GetDeviceContext(FxDevice);

	WdfTimerStop(pDevice->Timer, TRUE);

	return STATUS_SUCCESS;
}

NTSTATUS
CrosSensorsEvtDeviceAdd(
	IN WDFDRIVER       Driver,
	IN PWDFDEVICE_INIT DeviceInit
	)
{
	NTSTATUS                      status = STATUS_SUCCESS;
	WDF_IO_QUEUE_CONFIG           queueConfig;
	WDF_OBJECT_ATTRIBUTES         attributes;
	WDFDEVICE                     device;
	WDF_INTERRUPT_CONFIG interruptConfig;
	WDFQUEUE                      queue;
	UCHAR                         minorFunction;
	PCROSSENSORS_CONTEXT               devContext;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_PNP,
		"CrosSensorsEvtDeviceAdd called\n");

	//
	// Tell framework this is a filter driver. Filter drivers by default are  
	// not power policy owners. This works well for this driver because
	// HIDclass driver is the power policy owner for HID minidrivers.
	//

	WdfFdoInitSetFilter(DeviceInit);

	{
		WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
		WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

		pnpCallbacks.EvtDevicePrepareHardware = OnPrepareHardware;
		pnpCallbacks.EvtDeviceReleaseHardware = OnReleaseHardware;
		pnpCallbacks.EvtDeviceSelfManagedIoInit = OnSelfManagedIoInit;
		pnpCallbacks.EvtDeviceD0Entry = OnD0Entry;
		pnpCallbacks.EvtDeviceD0Exit = OnD0Exit;

		WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);
	}

	//
	// Setup the device context
	//

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, CROSSENSORS_CONTEXT);

	//
	// Create a framework device object.This call will in turn create
	// a WDM device object, attach to the lower stack, and set the
	// appropriate flags and attributes.
	//

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);

	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceCreate failed with status code 0x%x\n", status);

		return status;
	}

	{
		WDF_DEVICE_STATE deviceState;
		WDF_DEVICE_STATE_INIT(&deviceState);

		deviceState.NotDisableable = WdfFalse;
		WdfDeviceSetDeviceState(device, &deviceState);
	}

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);

	queueConfig.EvtIoInternalDeviceControl = CrosSensorsEvtInternalDeviceControl;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue
		);

	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	//
	// Create manual I/O queue to take care of hid report read requests
	//

	devContext = GetDeviceContext(device);
	devContext->FxDevice = device;

	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

	queueConfig.PowerManaged = WdfTrue;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->ReportQueue
		);

	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	//
	// Create manual I/O queue to take care of idle power requests
	//

	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

	queueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->IdleQueue
	);

	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	WDF_TIMER_CONFIG              timerConfig;
	WDFTIMER                      hTimer;

	WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, SensorTimerFunc, 10);

	timerConfig.TolerableDelay = TolerableDelayUnlimited; //Don't wake from S0ix

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = device;
	status = WdfTimerCreate(&timerConfig, &attributes, &hTimer);
	devContext->Timer = hTimer;
	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_PNP, "(%!FUNC!) WdfTimerCreate failed status:%!STATUS!\n", status);
		return status;
	}

	//
	// Initialize DeviceMode
	//

	devContext->TabletMode = FALSE;

	return status;
}

void
CrosSensorsIdleIrpWorkItem
(
	IN WDFWORKITEM IdleWorkItem
)
{
	NTSTATUS status;
	PIDLE_WORKITEM_CONTEXT idleWorkItemContext;
	PCROSSENSORS_CONTEXT deviceContext;
	PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO idleCallbackInfo;

	idleWorkItemContext = GetIdleWorkItemContext(IdleWorkItem);
	NT_ASSERT(idleWorkItemContext != NULL);

	deviceContext = GetDeviceContext(idleWorkItemContext->FxDevice);
	NT_ASSERT(deviceContext != NULL);

	//
	// Get the idle callback info from the workitem context
	//
	PIRP irp = WdfRequestWdmGetIrp(idleWorkItemContext->FxRequest);
	PIO_STACK_LOCATION stackLocation = IoGetCurrentIrpStackLocation(irp);

	idleCallbackInfo = (PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO)
		(stackLocation->Parameters.DeviceIoControl.Type3InputBuffer);

	//
	// idleCallbackInfo is validated already, so invoke idle callback
	//
	idleCallbackInfo->IdleCallback(idleCallbackInfo->IdleContext);

	//
	// Park this request in our IdleQueue and mark it as pending
	// This way if the IRP was cancelled, WDF will cancel it for us
	//
	status = WdfRequestForwardToIoQueue(
		idleWorkItemContext->FxRequest,
		deviceContext->IdleQueue);

	if (!NT_SUCCESS(status))
	{
		//
		// IdleQueue is a manual-dispatch, non-power-managed queue. This should
		// *never* fail.
		//

		NT_ASSERTMSG("WdfRequestForwardToIoQueue to IdleQueue failed!", FALSE);

		CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Error forwarding idle notification Request:0x%p to IdleQueue:0x%p - %!STATUS!",
			idleWorkItemContext->FxRequest,
			deviceContext->IdleQueue,
			status);

		//
		// Complete the request if we couldnt forward to the Idle Queue
		//
		WdfRequestComplete(idleWorkItemContext->FxRequest, status);
	}
	else
	{
		CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Forwarded idle notification Request:0x%p to IdleQueue:0x%p - %!STATUS!",
			idleWorkItemContext->FxRequest,
			deviceContext->IdleQueue,
			status);
	}

	//
	// Delete the workitem since we're done with it
	//
	WdfObjectDelete(IdleWorkItem);

	return;
}

NTSTATUS
CrosSensorsProcessIdleRequest(
	IN PCROSSENSORS_CONTEXT pDevice,
	IN WDFREQUEST Request,
	OUT BOOLEAN* Complete
)
{
	PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO idleCallbackInfo;
	PIRP irp;
	PIO_STACK_LOCATION irpSp;
	NTSTATUS status;

	NT_ASSERT(Complete != NULL);
	*Complete = TRUE;

	//
	// Retrieve request parameters and validate
	//
	irp = WdfRequestWdmGetIrp(Request);
	irpSp = IoGetCurrentIrpStackLocation(irp);

	if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
		sizeof(HID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO))
	{
		status = STATUS_INVALID_BUFFER_SIZE;

		CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Error: Input buffer is too small to process idle request - %!STATUS!",
			status);

		goto exit;
	}

	//
	// Grab the callback
	//
	idleCallbackInfo = (PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO)
		irpSp->Parameters.DeviceIoControl.Type3InputBuffer;

	NT_ASSERT(idleCallbackInfo != NULL);

	if (idleCallbackInfo == NULL || idleCallbackInfo->IdleCallback == NULL)
	{
		status = STATUS_NO_CALLBACK_ACTIVE;
		CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Error: Idle Notification request %p has no idle callback info - %!STATUS!",
			Request,
			status);
		goto exit;
	}

	{
		//
		// Create a workitem for the idle callback
		//
		WDF_OBJECT_ATTRIBUTES workItemAttributes;
		WDF_WORKITEM_CONFIG workitemConfig;
		WDFWORKITEM idleWorkItem;
		PIDLE_WORKITEM_CONTEXT idleWorkItemContext;

		WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&workItemAttributes, IDLE_WORKITEM_CONTEXT);
		workItemAttributes.ParentObject = pDevice->FxDevice;

		WDF_WORKITEM_CONFIG_INIT(&workitemConfig, CrosSensorsIdleIrpWorkItem);

		status = WdfWorkItemCreate(
			&workitemConfig,
			&workItemAttributes,
			&idleWorkItem
		);

		if (!NT_SUCCESS(status)) {
			CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
				"Error creating creating idle work item - %!STATUS!",
				status);
			goto exit;
		}

		//
		// Set the workitem context
		//
		idleWorkItemContext = GetIdleWorkItemContext(idleWorkItem);
		idleWorkItemContext->FxDevice = pDevice->FxDevice;
		idleWorkItemContext->FxRequest = Request;

		//
		// Enqueue a workitem for the idle callback
		//
		WdfWorkItemEnqueue(idleWorkItem);

		//
		// Mark the request as pending so that 
		// we can complete it when we come out of idle
		//
		*Complete = FALSE;
	}

exit:

	return status;
}

VOID
CrosSensorsCompleteIdleIrp(
	IN PCROSSENSORS_CONTEXT FxDeviceContext
)
/*++

Routine Description:

	This is invoked when we enter D0.
	We simply complete the Idle Irp if it hasn't been cancelled already.

Arguments:

	FxDeviceContext -  Pointer to Device Context for the device

Return Value:



--*/
{
	NTSTATUS status;
	WDFREQUEST request = NULL;

	//
	// Lets try to retrieve the Idle IRP from the Idle queue
	//
	status = WdfIoQueueRetrieveNextRequest(
		FxDeviceContext->IdleQueue,
		&request);

	//
	// We did not find the Idle IRP, maybe it was cancelled
	// 
	if (!NT_SUCCESS(status) || (request == NULL))
	{
		CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Error finding idle notification request in IdleQueue:0x%p - %!STATUS!",
			FxDeviceContext->IdleQueue,
			status);
	}
	else
	{
		//
		// Complete the Idle IRP
		//
		WdfRequestComplete(request, status);

		CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Completed idle notification Request:0x%p from IdleQueue:0x%p - %!STATUS!",
			request,
			FxDeviceContext->IdleQueue,
			status);
	}

	return;
}

VOID
CrosSensorsEvtInternalDeviceControl(
	IN WDFQUEUE     Queue,
	IN WDFREQUEST   Request,
	IN size_t       OutputBufferLength,
	IN size_t       InputBufferLength,
	IN ULONG        IoControlCode
	)
{
	NTSTATUS            status = STATUS_SUCCESS;
	WDFDEVICE           device;
	PCROSSENSORS_CONTEXT     devContext;
	BOOLEAN             completeRequest = TRUE;

	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	device = WdfIoQueueGetDevice(Queue);
	devContext = GetDeviceContext(device);

	CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
		"%s, Queue:0x%p, Request:0x%p\n",
		DbgHidInternalIoctlString(IoControlCode),
		Queue,
		Request
		);

	//
	// Please note that HIDCLASS provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl. So depending on the ioctl code, we will either
	// use retreive function or escape to WDM to get the UserBuffer.
	//

	switch (IoControlCode)
	{

	case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
		//
		// Retrieves the device's HID descriptor.
		//
		status = CrosSensorsGetHidDescriptor(device, Request);
		break;

	case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
		//
		//Retrieves a device's attributes in a HID_DEVICE_ATTRIBUTES structure.
		//
		status = CrosSensorsGetDeviceAttributes(Request);
		break;

	case IOCTL_HID_GET_REPORT_DESCRIPTOR:
		//
		//Obtains the report descriptor for the HID device.
		//
		status = CrosSensorsGetReportDescriptor(device, Request);
		break;

	case IOCTL_HID_GET_STRING:
		//
		// Requests that the HID minidriver retrieve a human-readable string
		// for either the manufacturer ID, the product ID, or the serial number
		// from the string descriptor of the device. The minidriver must send
		// a Get String Descriptor request to the device, in order to retrieve
		// the string descriptor, then it must extract the string at the
		// appropriate index from the string descriptor and return it in the
		// output buffer indicated by the IRP. Before sending the Get String
		// Descriptor request, the minidriver must retrieve the appropriate
		// index for the manufacturer ID, the product ID or the serial number
		// from the device extension of a top level collection associated with
		// the device.
		//
		status = CrosSensorsGetString(Request);
		break;

	case IOCTL_HID_WRITE_REPORT:
	case IOCTL_HID_SET_OUTPUT_REPORT:
		//
		//Transmits a class driver-supplied report to the device.
		//
		status = CrosSensorsWriteReport(devContext, Request);
		break;

	case IOCTL_HID_READ_REPORT:
	case IOCTL_HID_GET_INPUT_REPORT:
		//
		// Returns a report from the device into a class driver-supplied buffer.
		// 
		status = CrosSensorsReadReport(devContext, Request, &completeRequest);
		break;

	case IOCTL_HID_SET_FEATURE:
		//
		// This sends a HID class feature report to a top-level collection of
		// a HID class device.
		//
		status = CrosSensorsSetFeature(devContext, Request, &completeRequest);
		break;

	case IOCTL_HID_GET_FEATURE:
		//
		// returns a feature report associated with a top-level collection
		//
		status = CrosSensorsGetFeature(devContext, Request, &completeRequest);
		break;

	case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
		//Handle HID Idle request
		status = CrosSensorsProcessIdleRequest(devContext, Request, &completeRequest);
		break;

	case IOCTL_HID_ACTIVATE_DEVICE:
		//
		// Makes the device ready for I/O operations.
		//
	case IOCTL_HID_DEACTIVATE_DEVICE:
		//
		// Causes the device to cease operations and terminate all outstanding
		// I/O requests.
		//
	default:
		status = STATUS_NOT_SUPPORTED;
		break;
	}

	if (completeRequest)
	{
		WdfRequestComplete(Request, status);

		CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"%s completed, Queue:0x%p, Request:0x%p\n",
			DbgHidInternalIoctlString(IoControlCode),
			Queue,
			Request
			);
	}
	else
	{
		CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"%s deferred, Queue:0x%p, Request:0x%p\n",
			DbgHidInternalIoctlString(IoControlCode),
			Queue,
			Request
			);
	}

	return;
}

NTSTATUS
CrosSensorsGetHidDescriptor(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request
	)
{
	NTSTATUS            status = STATUS_SUCCESS;
	size_t              bytesToCopy = 0;
	WDFMEMORY           memory;

	UNREFERENCED_PARAMETER(Device);

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetHidDescriptor Entry\n");

	//
	// This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
	// will correctly retrieve buffer from Irp->UserBuffer. 
	// Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl.
	//
	status = WdfRequestRetrieveOutputMemory(Request, &memory);

	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfRequestRetrieveOutputMemory failed 0x%x\n", status);

		return status;
	}

	//
	// Use hardcoded "HID Descriptor" 
	//
	bytesToCopy = DefaultHidDescriptor.bLength;

	if (bytesToCopy == 0)
	{
		status = STATUS_INVALID_DEVICE_STATE;

		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"DefaultHidDescriptor is zero, 0x%x\n", status);

		return status;
	}

	status = WdfMemoryCopyFromBuffer(memory,
		0, // Offset
		(PVOID)&DefaultHidDescriptor,
		bytesToCopy);

	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfMemoryCopyFromBuffer failed 0x%x\n", status);

		return status;
	}

	//
	// Report how many bytes were copied
	//
	WdfRequestSetInformation(Request, bytesToCopy);

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetHidDescriptor Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosSensorsGetReportDescriptor(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request
	)
{
	NTSTATUS            status = STATUS_SUCCESS;
	ULONG_PTR           bytesToCopy;
	WDFMEMORY           memory;

	UNREFERENCED_PARAMETER(Device);

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetReportDescriptor Entry\n");

	//
	// This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
	// will correctly retrieve buffer from Irp->UserBuffer. 
	// Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl.
	//
	status = WdfRequestRetrieveOutputMemory(Request, &memory);
	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfRequestRetrieveOutputMemory failed 0x%x\n", status);

		return status;
	}

	//
	// Use hardcoded Report descriptor
	//
	bytesToCopy = DefaultHidDescriptor.DescriptorList[0].wReportLength;

	if (bytesToCopy == 0)
	{
		status = STATUS_INVALID_DEVICE_STATE;

		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"DefaultHidDescriptor's reportLength is zero, 0x%x\n", status);

		return status;
	}

	status = WdfMemoryCopyFromBuffer(memory,
		0,
		(PVOID)DefaultReportDescriptor,
		bytesToCopy);
	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfMemoryCopyFromBuffer failed 0x%x\n", status);

		return status;
	}

	//
	// Report how many bytes were copied
	//
	WdfRequestSetInformation(Request, bytesToCopy);

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetReportDescriptor Exit = 0x%x\n", status);

	return status;
}


NTSTATUS
CrosSensorsGetDeviceAttributes(
	IN WDFREQUEST Request
	)
{
	NTSTATUS                 status = STATUS_SUCCESS;
	PHID_DEVICE_ATTRIBUTES   deviceAttributes = NULL;

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetDeviceAttributes Entry\n");

	//
	// This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
	// will correctly retrieve buffer from Irp->UserBuffer. 
	// Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl.
	//
	status = WdfRequestRetrieveOutputBuffer(Request,
		sizeof(HID_DEVICE_ATTRIBUTES),
		(PVOID *)&deviceAttributes,
		NULL);
	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);

		return status;
	}

	//
	// Set USB device descriptor
	//

	deviceAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);
	deviceAttributes->VendorID = CROSSENSORS_VID;
	deviceAttributes->ProductID = CROSSENSORS_PID;
	deviceAttributes->VersionNumber = CROSSENSORS_VERSION;

	//
	// Report how many bytes were copied
	//
	WdfRequestSetInformation(Request, sizeof(HID_DEVICE_ATTRIBUTES));

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetDeviceAttributes Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosSensorsGetString(
	IN WDFREQUEST Request
	)
{

	NTSTATUS status = STATUS_SUCCESS;
	PWSTR pwstrID;
	size_t lenID;
	WDF_REQUEST_PARAMETERS params;
	void *pStringBuffer = NULL;

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetString Entry\n");

	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);

	switch ((ULONG_PTR)params.Parameters.DeviceIoControl.Type3InputBuffer & 0xFFFF)
	{
	case HID_STRING_ID_IMANUFACTURER:
		pwstrID = L"CrosSensors.\0";
		break;

	case HID_STRING_ID_IPRODUCT:
		pwstrID = L"Chrome EC Sensor Hub\0";
		break;

	case HID_STRING_ID_ISERIALNUMBER:
		pwstrID = L"123123123\0";
		break;

	default:
		pwstrID = NULL;
		break;
	}

	lenID = pwstrID ? wcslen(pwstrID)*sizeof(WCHAR) + sizeof(UNICODE_NULL) : 0;

	if (pwstrID == NULL)
	{

		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosSensorsGetString Invalid request type\n");

		status = STATUS_INVALID_PARAMETER;

		return status;
	}

	status = WdfRequestRetrieveOutputBuffer(Request,
		lenID,
		&pStringBuffer,
		&lenID);

	if (!NT_SUCCESS(status))
	{

		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosSensorsGetString WdfRequestRetrieveOutputBuffer failed Status 0x%x\n", status);

		return status;
	}

	RtlCopyMemory(pStringBuffer, pwstrID, lenID);

	WdfRequestSetInformation(Request, lenID);

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetString Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosSensorsWriteReport(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN WDFREQUEST Request
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_REQUEST_PARAMETERS params;
	PHID_XFER_PACKET transferPacket = NULL;
	size_t bytesWritten = 0;

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsWriteReport Entry\n");

	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);

	if (params.Parameters.DeviceIoControl.InputBufferLength < sizeof(HID_XFER_PACKET))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosSensorsWriteReport Xfer packet too small\n");

		status = STATUS_BUFFER_TOO_SMALL;
	}
	else
	{

		transferPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;

		if (transferPacket == NULL)
		{
			CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"CrosSensorsWriteReport No xfer packet\n");

			status = STATUS_INVALID_DEVICE_REQUEST;
		}
		else
		{
			//
			// switch on the report id
			//

			switch (transferPacket->reportId)
			{
			default:
				CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
					"CrosSensorsWriteReport Unhandled report type %d\n", transferPacket->reportId);

				status = STATUS_INVALID_PARAMETER;

				break;
			}
		}
	}

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsWriteReport Exit = 0x%x\n", status);

	return status;

}

NTSTATUS
CrosSensorsProcessVendorReport(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN PVOID ReportBuffer,
	IN ULONG ReportBufferLen,
	OUT size_t* BytesWritten
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFREQUEST reqRead;
	PVOID pReadReport = NULL;
	size_t bytesReturned = 0;

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsProcessVendorReport Entry\n");

	status = WdfIoQueueRetrieveNextRequest(DevContext->ReportQueue,
		&reqRead);

	if (NT_SUCCESS(status))
	{
		status = WdfRequestRetrieveOutputBuffer(reqRead,
			ReportBufferLen,
			&pReadReport,
			&bytesReturned);

		if (NT_SUCCESS(status))
		{
			//
			// Copy ReportBuffer into read request
			//

			if (bytesReturned > ReportBufferLen)
			{
				bytesReturned = ReportBufferLen;
			}

			RtlCopyMemory(pReadReport,
				ReportBuffer,
				bytesReturned);

			//
			// Complete read with the number of bytes returned as info
			//

			WdfRequestCompleteWithInformation(reqRead,
				status,
				bytesReturned);

			CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
				"CrosSensorsProcessVendorReport %d bytes returned\n", bytesReturned);

			//
			// Return the number of bytes written for the write request completion
			//

			*BytesWritten = bytesReturned;

			CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
				"%s completed, Queue:0x%p, Request:0x%p\n",
				DbgHidInternalIoctlString(IOCTL_HID_READ_REPORT),
				DevContext->ReportQueue,
				reqRead);
		}
		else
		{
			CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"WdfRequestRetrieveOutputBuffer failed Status 0x%x\n", status);
		}
	}
	else
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfIoQueueRetrieveNextRequest failed Status 0x%x\n", status);
	}

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsProcessVendorReport Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosSensorsReadReport(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsReadReport Entry\n");

	//
	// Forward this read request to our manual queue
	// (in other words, we are going to defer this request
	// until we have a corresponding write request to
	// match it with)
	//

	status = WdfRequestForwardToIoQueue(Request, DevContext->ReportQueue);

	if (!NT_SUCCESS(status))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfRequestForwardToIoQueue failed Status 0x%x\n", status);
	}
	else
	{
		*CompleteRequest = FALSE;
	}

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsReadReport Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosSensorsSetFeature(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_REQUEST_PARAMETERS params;
	PHID_XFER_PACKET transferPacket = NULL;
	CrosSensorsFeatureReport* pReport = NULL;

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsSetFeature Entry\n");

	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);

	if (params.Parameters.DeviceIoControl.InputBufferLength < sizeof(HID_XFER_PACKET))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosSensorsSetFeature Xfer packet too small\n");

		status = STATUS_BUFFER_TOO_SMALL;
	}
	else
	{

		transferPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;

		if (transferPacket == NULL)
		{
			CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"CrosSensorsWriteReport No xfer packet\n");

			status = STATUS_INVALID_DEVICE_REQUEST;
		}
		else
		{
			//
			// switch on the report id
			//

			switch (transferPacket->reportId)
			{
			case REPORTID_ACCELEROMETER:
				break;
			default:

				CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
					"CrosSensorsSetFeature Unhandled report type %d\n", transferPacket->reportId);

				status = STATUS_INVALID_PARAMETER;

				break;
			}
		}
	}

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsSetFeature Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosSensorsGetFeature(
	IN PCROSSENSORS_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_REQUEST_PARAMETERS params;
	PHID_XFER_PACKET transferPacket = NULL;

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetFeature Entry\n");

	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);

	if (params.Parameters.DeviceIoControl.OutputBufferLength < sizeof(HID_XFER_PACKET))
	{
		CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosSensorsGetFeature Xfer packet too small\n");

		status = STATUS_BUFFER_TOO_SMALL;
	}
	else
	{

		transferPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;

		if (transferPacket == NULL)
		{
			CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"CrosSensorsGetFeature No xfer packet\n");

			status = STATUS_INVALID_DEVICE_REQUEST;
		}
		else
		{
			//
			// switch on the report id
			//

			switch (transferPacket->reportId)
			{
			case REPORTID_ACCELEROMETER:
				if (transferPacket->reportBufferLen >= sizeof(CrosSensorsFeatureReport))
				{
					CrosSensorsFeatureReport* pReport = (CrosSensorsFeatureReport*)transferPacket->reportBuffer;

					pReport->ConnectionType = SENSOR_PROPERTY_CONNECTION_TYPE_PC_INTEGRATED_SEL;
					pReport->ReportingState = SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_SEL;
					pReport->SensorState = SENSOR_STATE_READY_SEL;
					pReport->ReportInterval = 1;
					pReport->SensorDateMotionAccelerationModChangeSensitivityAbs = 1;
					pReport->SensorDateMotionAccelerationModMax = 32767;
					pReport->SensorDateMotionAccelerationModMin = -32767;

					status = STATUS_SUCCESS;

					CrosSensorsPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
						"CrosSensorsGetFeature MaximumCount = 0x%x\n", MULTI_MAX_COUNT);
				}
				else
				{
					status = STATUS_INVALID_PARAMETER;

					CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
						"CrosSensorsGetFeature Error transferPacket->reportBufferLen (%d) is different from sizeof(CrosSensorsFeatureReport) (%d)\n",
						transferPacket->reportBufferLen,
						sizeof(CrosSensorsFeatureReport));
				}
				break;
			default:

				CrosSensorsPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
					"CrosSensorsGetFeature Unhandled report type %d\n", transferPacket->reportId);

				status = STATUS_INVALID_PARAMETER;

				break;
			}
		}
	}

	CrosSensorsPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosSensorsGetFeature Exit = 0x%x\n", status);

	return status;
}

PCHAR
DbgHidInternalIoctlString(
	IN ULONG IoControlCode
	)
{
	switch (IoControlCode)
	{
	case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
		return "IOCTL_HID_GET_DEVICE_DESCRIPTOR";
	case IOCTL_HID_GET_REPORT_DESCRIPTOR:
		return "IOCTL_HID_GET_REPORT_DESCRIPTOR";
	case IOCTL_HID_READ_REPORT:
		return "IOCTL_HID_READ_REPORT";
	case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
		return "IOCTL_HID_GET_DEVICE_ATTRIBUTES";
	case IOCTL_HID_WRITE_REPORT:
		return "IOCTL_HID_WRITE_REPORT";
	case IOCTL_HID_SET_FEATURE:
		return "IOCTL_HID_SET_FEATURE";
	case IOCTL_HID_GET_FEATURE:
		return "IOCTL_HID_GET_FEATURE";
	case IOCTL_HID_GET_STRING:
		return "IOCTL_HID_GET_STRING";
	case IOCTL_HID_ACTIVATE_DEVICE:
		return "IOCTL_HID_ACTIVATE_DEVICE";
	case IOCTL_HID_DEACTIVATE_DEVICE:
		return "IOCTL_HID_DEACTIVATE_DEVICE";
	case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
		return "IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST";
	case IOCTL_HID_SET_OUTPUT_REPORT:
		return "IOCTL_HID_SET_OUTPUT_REPORT";
	case IOCTL_HID_GET_INPUT_REPORT:
		return "IOCTL_HID_GET_INPUT_REPORT";
	default:
		return "Unknown IOCTL";
	}
}
