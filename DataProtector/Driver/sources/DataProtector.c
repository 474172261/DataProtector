#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include "debug.h"
#include "UnicodeString.h"
#include "communication.h"
#include "Handler.h"
#include "Helper.h"
#include "FsFilter.h"
#include "UnnecessaryFunc.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

PFLT_FILTER ghFilter;
volatile LONG gEventCount = 0;

/*************************************************************************
Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
DPfUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
DPfPreCreateOperationCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
DPfPreSetInfoCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
DPfPreCleanUpOperationCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);
EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DPfUnload)
#pragma alloc_text(PAGE, DPfilterInstanceQueryTeardown)
#pragma alloc_text(PAGE, DPfilterInstanceSetup)
#pragma alloc_text(PAGE, DPfilterInstanceTeardownStart)
#pragma alloc_text(PAGE, DPfilterInstanceTeardownComplete)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
	{ IRP_MJ_CREATE,
		0,
		DPfPreCreateOperationCallback,
		NULL 
	},
	{ IRP_MJ_SET_INFORMATION,
		FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
		DPfPreSetInfoCallback,
		NULL 
	},
	//{ IRP_MJ_CLEANUP,
	//	0,
	//	DPfPreCleanUpOperationCallback,
	//	NULL 
	//},

	{ IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

	sizeof(FLT_REGISTRATION),         //  Size
	FLT_REGISTRATION_VERSION,           //  Version
	0,                                  //  Flags

	NULL,                               //  Context
	Callbacks,                          //  Operation callbacks

	DPfUnload,                           //  MiniFilterUnload

	NULL,                    //  InstanceSetup
	NULL,            //  InstanceQueryTeardown
	NULL,            //  InstanceTeardownStart
	NULL,         //  InstanceTeardownComplete

	NULL,                               //  GenerateFileName
	NULL,                               //  GenerateDestinationFileName
	NULL                                //  NormalizeNameComponent

};



NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status;
	UNREFERENCED_PARAMETER(RegistryPath);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DPfilter!DriverEntry: Entered\n"));
	//  Register with FltMgr to tell it our callback routines
	status = FltRegisterFilter(DriverObject,
		&FilterRegistration,
		&ghFilter);

	FLT_ASSERT(NT_SUCCESS(status));

	gEventCount = 0;

	if (NT_SUCCESS(status)) {
		status = InitFilterUnicodeString();
		if (NT_SUCCESS(status)) {
			status = InitBasicUnicodeString();
			if (NT_SUCCESS(status)) {
				status = InitCommunication();
				if (NT_SUCCESS(status)) {
					//  Start filtering i/o
					status = FltStartFiltering(ghFilter);
				}
			}
		}
	}
	
	if (!NT_SUCCESS(status)) {
		UninitCommunication();
		UnInitBasicUnicodeString();
		UnInitFilterUnicodeString();
		FltUnregisterFilter(ghFilter);// this must at last
	}

	return status;
}

NTSTATUS
DPfUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DPfilter!DPfilterUnload: Entered\n"));

	UninitCommunication();
	UnInitBasicUnicodeString();
	UnInitFilterUnicodeString();
	FltUnregisterFilter(ghFilter);

	return STATUS_SUCCESS;
}


/*************************************************************************
MiniFilter callback routines.
*************************************************************************/

FLT_PREOP_CALLBACK_STATUS
DPfPreCreateOperationCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(FltObjects);

	FLT_PREOP_CALLBACK_STATUS retstatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	UINT32 operation = 0;

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DPfilter!DPfilterPreOperation: Entered\n"));

	PAGED_CODE();

	if (!gpClientPort || !gpServerPort) {
		goto L_EXIT;
	}

	InterlockedIncrement(&gEventCount);// eventcount for safe release memory.
	
	if (FlagOn(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess, FILE_WRITE_DATA)
		&& !FlagOnHigh(Data->Iopb->Parameters.Create.Options, FILE_CREATE)) {
		operation = HANDLE_OPERATION_OVERWRITE;
	}
	else if (FlagOn(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess, FILE_DELETE_ON_CLOSE)) {
		operation = HANDLE_OPERATION_DELETE;
	}
	if(operation){
		retstatus = HandleOperation(Data, operation);
	}
	
	InterlockedDecrement(&gEventCount);
L_EXIT:
	*CompletionContext = NULL;
	return retstatus;
}

FLT_PREOP_CALLBACK_STATUS
DPfPreSetInfoCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	FLT_PREOP_CALLBACK_STATUS retstatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	UNREFERENCED_PARAMETER(FltObjects);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DPfilter!DPfPreSetInfoCallback: Entered\n"));

	PAGED_CODE();

	if (!gpClientPort || !gpServerPort) {
		goto L_EXIT;
	}
	InterlockedIncrement(&gEventCount);// eventcount for safe release memory.

	switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass) {

	case FileDispositionInformation:
	case FileDispositionInformationEx:
		retstatus = HandleOperation(Data, HANDLE_OPERATION_DELETE);
		break;
	default:
		break;
	}
	InterlockedDecrement(&gEventCount);

L_EXIT:
	*CompletionContext = NULL;
	return retstatus;
}
//
//FLT_PREOP_CALLBACK_STATUS
//DPfPreCleanUpOperationCallback(
//	_Inout_ PFLT_CALLBACK_DATA Data,
//	_In_ PCFLT_RELATED_OBJECTS FltObjects,
//	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
//) 
//{
//	UNREFERENCED_PARAMETER(Data);
//	UNREFERENCED_PARAMETER(FltObjects);
//
//	PAGED_CODE();
//
//	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
//		("DPfilter!DPfPreCleanUpOperationCallback: Entered.\n"));
//
//	*CompletionContext = NULL;
//	return FLT_PREOP_SUCCESS_NO_CALLBACK;
//}