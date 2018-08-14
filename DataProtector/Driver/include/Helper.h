#pragma once

#include <Ntddk.h>



PUNICODE_STRING MyAllocUnicodeString(USHORT size);

void MyFreeUnicodeString(PUNICODE_STRING pUnicode);

typedef NTSTATUS(NTAPI *ZWQUERYINFORMATIONPROCESS)(
	_In_      HANDLE           ProcessHandle,
	_In_      PROCESSINFOCLASS ProcessInformationClass,
	_Out_     PVOID            ProcessInformation,
	_In_      ULONG            ProcessInformationLength,
	_Out_opt_ PULONG           ReturnLength
	);

NTSTATUS GetVolumeNameByName(WCHAR *name, PUNICODE_STRING *outVolumeName);

NTSTATUS DoTerminateProcess(HANDLE ProcessID);

NTSTATUS
GetProcessImageName(
	PEPROCESS eProcess,
	PUNICODE_STRING* ppProcessImageName
);

PUNICODE_STRING ConcatUnicodeString(
	PUNICODE_STRING pU1,
	PUNICODE_STRING pU2
);

NTSTATUS GetFilePath(_In_ PFLT_CALLBACK_DATA Data, PUNICODE_STRING *ppFilePath);