#include <fltKernel.h>
#include "debug.h"

NTSTATUS
DPfilterInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
/*++
Routine Description:
This routine is called whenever a new instance is created on a volume. This
gives us a chance to decide if we need to attach to this volume or not.
New instances are only created and attached to a volume if it is a writable
NTFS or ReFS volume.
Arguments:
FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance and its associated volume.
Flags - Flags describing the reason for this attach request.
VolumeFilesystemType - A FLT_FSTYPE_* value indicating which file system type
the Filter Manager is offering to attach us to.
Return Value:
STATUS_SUCCESS - attach
STATUS_FLT_DO_NOT_ATTACH - do not attach
--*/
{
	NTSTATUS status = STATUS_SUCCESS;
	BOOLEAN isWritable = FALSE;

	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("delete!DfInstanceSetup: Entered\n"));

	status = FltIsVolumeWritable(FltObjects->Volume,
		&isWritable);

	if (!NT_SUCCESS(status)) {

		return STATUS_FLT_DO_NOT_ATTACH;
	}

	//
	//  Attaching to read-only volumes is pointless as you should not be able
	//  to delete files on such a volume.
	//

	if (isWritable) {

		switch (VolumeFilesystemType) {

		case FLT_FSTYPE_NTFS:
		case FLT_FSTYPE_REFS:

			status = STATUS_SUCCESS;
			break;

		default:

			return STATUS_FLT_DO_NOT_ATTACH;
		}

	}
	else {

		return STATUS_FLT_DO_NOT_ATTACH;
	}

	return status;
}

NTSTATUS
DPfilterInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("delete!DfInstanceQueryTeardown: Entered\n"));

	return STATUS_SUCCESS;
}


VOID
DPfilterInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++
Routine Description:
This routine is called at the start of instance teardown.
Arguments:
FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance and its associated volume.
Flags - Reason why this instance is been deleted.
Return Value:
None.
--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("delete!DfInstanceTeardownStart: Entered\n"));
}


VOID
DPfilterInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++
Routine Description:
This routine is called at the end of instance teardown.
Arguments:
FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance and its associated volume.
Flags - Reason why this instance is been deleted.
Return Value:
None.
--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("delete!DfInstanceTeardownComplete: Entered\n"));
}
