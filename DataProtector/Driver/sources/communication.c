#include <fltKernel.h>
#include "FsFilter.h"
#include "communication.h"
#include "debug.h"

PFLT_PORT gpServerPort = NULL;
PFLT_PORT gpClientPort = NULL;


NTSTATUS
CommunicateConnect(
	IN PFLT_PORT ClientPort,
	IN PVOID ServerPortCookie,
	IN PVOID ConnectionContext,
	IN ULONG SizeOfContext,
	OUT PVOID *ConnectionPortCookie
) {
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionPortCookie);
	PAGED_CODE();
	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DPfilter!CommunicateConnect client connect:%p\n", ClientPort));
	gpClientPort = ClientPort;
	return STATUS_SUCCESS;
}

VOID
CommunicateDisconnect(
	IN PVOID ConnectionCookie
) {
	UNREFERENCED_PARAMETER(ConnectionCookie);
	PAGED_CODE();
	FltCloseClientPort(ghFilter, &gpClientPort);
	gpClientPort = NULL;
}



NTSTATUS InitCommunication(VOID) {
	UNICODE_STRING uniString;
	PSECURITY_DESCRIPTOR sd;
	OBJECT_ATTRIBUTES oa;
	NTSTATUS status;

	RtlInitUnicodeString(&uniString, GUARD_RANSOMWARE_PORT_NAME);

	status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);

	if (NT_SUCCESS(status)) {

		InitializeObjectAttributes(&oa,
			&uniString,
			OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
			NULL,
			sd);

		status = FltCreateCommunicationPort(ghFilter, &gpServerPort,
			&oa,
			NULL,
			CommunicateConnect,
			CommunicateDisconnect,
			NULL,
			1
		);

		FltFreeSecurityDescriptor(sd);
		if (!NT_SUCCESS(status)) {
			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
				("DPfilter!FltCreateCommunicationPort fail.status:%x\n", status));
		}
	}
	else {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!FltBuildDefaultSecurityDescriptor fail.status:%x\n", status));
	}
	return status;
}

VOID UninitCommunication() {
	LARGE_INTEGER timeout;

	if (gpServerPort) {
		//Notice! any client port for that connection will remain after close serverport.
		FltCloseCommunicationPort(gpServerPort); 
		gpServerPort = NULL;
		timeout.QuadPart = Int32x32To64(-1, 1000 * 1000 * 10); // 1s
		while (gEventCount) {
			KeDelayExecutionThread(KernelMode,
				FALSE,
				&timeout);
		}
	}
}