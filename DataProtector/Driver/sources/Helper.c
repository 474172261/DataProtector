#include <fltKernel.h>
#include "FsFilter.h"
#include "debug.h"
#include "Helper.h"

#pragma warning(disable:4055)
#pragma warning(disable:4242)
#pragma warning(disable:4244)

PUNICODE_STRING MyAllocUnicodeString(USHORT size) {
	WCHAR *buf;
	PUNICODE_STRING pUnicode = ExAllocatePoolWithTag(
		NonPagedPool,
		sizeof(UNICODE_STRING),
		POOL_TAG_UNICODESTR
	);
	if (!pUnicode)
		return NULL;

	buf = ExAllocatePoolWithTag(
		NonPagedPool,
		size + sizeof(WCHAR),
		POOL_TAG_UNICODEBUF
	);

	if (!buf) {
		ExFreePoolWithTag(pUnicode, POOL_TAG_UNICODESTR);
		return NULL;
	}

	RtlZeroMemory(buf, size + sizeof(WCHAR));
	pUnicode->Length = size;
	pUnicode->MaximumLength = size + sizeof(WCHAR);
	pUnicode->Buffer = buf;
	return pUnicode;
}

void MyFreeUnicodeString(PUNICODE_STRING pUnicode) {
	ExFreePoolWithTag(pUnicode->Buffer, POOL_TAG_UNICODEBUF);
	ExFreePoolWithTag(pUnicode, POOL_TAG_UNICODESTR);
}

PUNICODE_STRING ConcatUnicodeString(
	PUNICODE_STRING pU1,
	PUNICODE_STRING pU2
) {
	USHORT len = (pU1->Length) + (pU2->Length);
	PUNICODE_STRING puStr = MyAllocUnicodeString(len);
	if (puStr) {
		RtlCopyMemory(puStr->Buffer, pU1->Buffer, pU1->Length);
		RtlCopyMemory((char *)puStr->Buffer + pU1->Length, pU2->Buffer, pU2->Length);
		*(WCHAR*)((char*)puStr->Buffer + len) = L'\0';
	}
	return puStr;
}

NTSTATUS GetVolumeNameByName(WCHAR *name, PUNICODE_STRING *outVolumeName) {
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	UNICODE_STRING * puVolumeName;
	UNICODE_STRING uVolumeName;
	ULONG ulVolumeNameSize;
	PFLT_VOLUME pVolume;

	RtlInitUnicodeString(&uVolumeName, name);
	status = FltGetVolumeFromName(ghFilter, &uVolumeName, &pVolume);
	if (!NT_SUCCESS(status)) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!FltGetVolumeFromName fail,0x%x", status));
		return status;
	}

	status = FltGetVolumeName(pVolume, NULL, &ulVolumeNameSize);
	if (status != STATUS_BUFFER_TOO_SMALL) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!FltGetVolumeName fail,0x%x", status));
		FltObjectDereference(pVolume);
		return status;
	}
	
	puVolumeName = MyAllocUnicodeString(ulVolumeNameSize);
	if (!puVolumeName) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!FltGetVolumeNameByName fail to alloc unicode string"));
		FltObjectDereference(pVolume);
		status = STATUS_UNSUCCESSFUL;
		return status;
	}

	status = FltGetVolumeName(pVolume, puVolumeName, NULL);
	if (!NT_SUCCESS(status)) {

		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!FltGetVolumeName fail,0x%x", status));

		FltObjectDereference(pVolume);
		MyFreeUnicodeString(puVolumeName);
		return status;
	}

	*outVolumeName = puVolumeName;

	FltObjectDereference(pVolume);
	return status;
}


NTSTATUS DoTerminateProcess(HANDLE ProcessID)
{
	NTSTATUS         ntStatus = STATUS_SUCCESS;
	HANDLE           hProcess;
	OBJECT_ATTRIBUTES ObjectAttributes;
	CLIENT_ID        ClientId;

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("drvTerminateProcess( %u )", ProcessID));

	InitializeObjectAttributes(&ObjectAttributes, NULL, OBJ_INHERIT, NULL, NULL);

	ClientId.UniqueProcess = (HANDLE)ProcessID;
	ClientId.UniqueThread = NULL;

	ntStatus = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &ObjectAttributes, &ClientId);
	if (NT_SUCCESS(ntStatus))
	{
		ntStatus = ZwTerminateProcess(hProcess, 0);
		if (!NT_SUCCESS(ntStatus)) {
			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
				("ZwTerminateProcess failed with status : %08X\n", ntStatus));
			ntStatus = STATUS_UNSUCCESSFUL;
		}
		ZwClose(hProcess);
	}
	else {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("ZwOpenProcess failed with status : %08X\n", ntStatus));
		ntStatus = STATUS_UNSUCCESSFUL;
	}
	return ntStatus;

}

NTSTATUS
GetProcessImageName(
	PEPROCESS eProcess,
	PUNICODE_STRING* ppProcessImageName
)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	ULONG returnedLength;
	PUNICODE_STRING pProcImgName = NULL;
	HANDLE hProcess = NULL;

	PAGED_CODE(); // this eliminates the possibility of the IDLE Thread/Process

	if (eProcess == NULL)
	{
		return STATUS_INVALID_PARAMETER_1;
	}

	status = ObOpenObjectByPointer(eProcess,
		0, NULL, 0, 0, KernelMode, &hProcess);

	if (!NT_SUCCESS(status))
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("ObOpenObjectByPointer Failed: %08x\n", status));
		return status;
	}

	UNICODE_STRING routineName = RTL_CONSTANT_STRING(L"ZwQueryInformationProcess");

	ZWQUERYINFORMATIONPROCESS fpZwQueryInformationProcess =
		(ZWQUERYINFORMATIONPROCESS)MmGetSystemRoutineAddress(&routineName);

	if (fpZwQueryInformationProcess == NULL)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("Cannot resolve ZwQueryInformationProcess\n"));
		status = STATUS_UNSUCCESSFUL;
		goto cleanUp;
	}

	/* Query the actual size of the process path */
	status = fpZwQueryInformationProcess(hProcess,
		ProcessImageFileName,
		NULL, // buffer
		0,    // buffer size
		&returnedLength);

	if (STATUS_INFO_LENGTH_MISMATCH != status) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("ZwQueryInformationProcess status = %x\n", status));
		goto cleanUp;
	}

	pProcImgName = ExAllocatePoolWithTag(PagedPool, returnedLength, POOL_TAG_IMGNAME);

	if (pProcImgName == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanUp;
	}

	/* Retrieve the process path from the handle to the process */
	status = fpZwQueryInformationProcess(hProcess,
		ProcessImageFileName,
		pProcImgName,
		returnedLength,
		&returnedLength);

	if (!NT_SUCCESS(status)) {
		ExFreePoolWithTag(pProcImgName, POOL_TAG_IMGNAME);
		pProcImgName = NULL;
	}

cleanUp:
	*ppProcessImageName = pProcImgName;
	ZwClose(hProcess);

	return status;
}

// convert \device\harddiskvolumes -> C:
// remembert to free FilePath's buffer by MyFreeUnicodeString
NTSTATUS GetFilePath(PFLT_CALLBACK_DATA Data, PUNICODE_STRING *ppFilePath) {
	UNICODE_STRING DosName;
	NTSTATUS status = IoVolumeDeviceToDosName(Data->Iopb->TargetFileObject->DeviceObject, &DosName);
	if (NT_SUCCESS(status)) {

		PUNICODE_STRING pFilePath = ConcatUnicodeString(
			&Data->Iopb->TargetFileObject->FileName,
			&DosName);

		if (pFilePath != NULL) {
			*ppFilePath = pFilePath;
		}
		else {
			status = STATUS_INSUFFICIENT_RESOURCES;
		}
		ExFreePool(DosName.Buffer);
	}

	return status;
}