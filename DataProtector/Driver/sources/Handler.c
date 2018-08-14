#include <fltKernel.h>
#include "Handler.h"
#include "debug.h"
#include "Helper.h"
#include "FsFilter.h"
#include "UnicodeString.h"
#include "communication.h"

#pragma warning(disable:4047)

int FilterWhiteImagePath(UNICODE_STRING *puImagePath) {
	PUNICODE_STRING *ppUnicode = guWhiteList.ppUnicodeStrArray;
	for (int i = 0; i < guWhiteList.count; i++) {
		if (RtlEqualUnicodeString(puImagePath, ppUnicode[i], TRUE)) {
			return TRUE;
		}
	}
	return FALSE;
}

int FilterWhiteDir(UNICODE_STRING *puFilePath) {
	PUNICODE_STRING *ppUnicode = guWhiteListDir.ppUnicodeStrArray;
	for (int i = 0; i < guWhiteList.count; i++) {
		if (RtlPrefixUnicodeString(ppUnicode[i], puFilePath, TRUE)) {
			return TRUE;
		}
	}
	return FALSE;
}

UINT32 HandleEvents(
	UNICODE_STRING *puFilePath, 
	UNICODE_STRING *puImgPath, 
	PEPROCESS pEprocess, 
	UINT32 operation
) 
{
	// backup file.
	NTSTATUS status;
	ULONG rlyLen;
	PPROTECTOR_NOTIFICATION pMessage = NULL;
	UINT32 code = REPLY_CODE_NONE;
	LARGE_INTEGER timeout = { 0 };
	
	// check path length avoid overwrite.
	if (puFilePath->Length >= MSG_PATH_MAX_SIZE*2
		|| puImgPath->Length >= MSG_PATH_MAX_SIZE*2) 
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!HandleEvents Path length is too big to send;filepathLen:%x,ImgFilepathLen:%x.\n",
				puFilePath->Length, puImgPath->Length)
		);
		return code;
	}

	pMessage = ExAllocatePoolWithTag(NonPagedPool,
		sizeof(PROTECTOR_NOTIFICATION),
		POOL_TAG_MSG);

	if (!pMessage) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!HandleEvents Can't alloc for message")
		);
		return code;
	}

	RtlZeroMemory(pMessage, sizeof(PROTECTOR_NOTIFICATION));
	
	pMessage->pid = PsGetProcessId(pEprocess);
	pMessage->operation = operation;
	// notice ,buffer is MSG_PATH_MAX_SIZE 400, check length before copy
	RtlCopyMemory(pMessage->fileName, puFilePath->Buffer, puFilePath->Length);
	RtlCopyMemory(pMessage->imgName, puImgPath->Buffer, puImgPath->Length);

	rlyLen = sizeof(FILTER_REPLY_HEADER) + sizeof(PROTECTOR_REPLY);
	timeout.QuadPart = Int32x32To64(-30, 1000 * 1000 * 10); // 30s

	status = FltSendMessage(ghFilter,
		&gpClientPort,
		pMessage,
		sizeof(PROTECTOR_NOTIFICATION),
		pMessage,
		&rlyLen,
		&timeout);// wait 30s. 

	if (NT_SUCCESS(status)) {
		if (status == STATUS_TIMEOUT){
			code = REPLY_CODE_STOPPROCESS;
		}
		else {
			code = ((PROTECTOR_REPLY*)pMessage)->code;
		}
	}
	if (code == REPLY_CODE_STOPPROCESS) {
		DoTerminateProcess(PsGetProcessId(pEprocess));
	}

	if(pMessage)
		ExFreePoolWithTag(pMessage, POOL_TAG_MSG);

	return code;
}


int FilterFileTypeAndWhiteList(
	PFLT_CALLBACK_DATA Data,
	UNICODE_STRING *puImgPath,
	PFLT_FILE_NAME_INFORMATION *ppFileNameInfo
) 
{
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION pFileNameInfo = NULL;
	UNICODE_STRING *pUnicodeStr = guExtFltTypes.pUnicodeStr;
	int needFilter = FALSE;

	status = FltGetFileNameInformation(
		Data, 
		FLT_FILE_NAME_NORMALIZED, 
		&pFileNameInfo // remember to free it out of this function.
	);

	if (!NT_SUCCESS(status)) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!FltGetFileNameInformation fail! %x.\n", status));
		return needFilter;
	}
	
	*ppFileNameInfo = pFileNameInfo;
	
	status = FltParseFileNameInformation(pFileNameInfo);
	if (NT_SUCCESS(status)) {
		for (int i = 0; i < guExtFltTypes.count; i++) {
			if (RtlEqualUnicodeString(&pFileNameInfo->Extension, &pUnicodeStr[i], FALSE)) {
				needFilter = TRUE;
				break;
			}
		}
	}
	else {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!FltParseFileNameInformation fail! %x.\n", status));
	}

	if (needFilter) {

		if (FilterWhiteDir(&pFileNameInfo->Name)) {
			needFilter = FALSE;
		}
		if (FilterWhiteImagePath(puImgPath)) {
			needFilter = FALSE;
		}
	}

	return needFilter;
}

FLT_PREOP_CALLBACK_STATUS HandleOperation(PFLT_CALLBACK_DATA Data, UINT32 operation) {
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	PEPROCESS pEprocess;
	NTSTATUS status;
	UNICODE_STRING *puImgPath;
	FLT_PREOP_CALLBACK_STATUS retstatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	pEprocess = IoThreadToProcess(Data->Thread);
	status = GetProcessImageName(pEprocess, &puImgPath);// remember to free this memory
	if (!NT_SUCCESS(status)) {

		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("Can't get imageName:0x%x", status));
		return retstatus;
	}

	if (FilterFileTypeAndWhiteList(Data, puImgPath, &pNameInfo)) {

		UINT32 ret = HandleEvents(&pNameInfo->Name,
			puImgPath,
			pEprocess,
			operation);

		if (ret == REPLY_CODE_STOPPROCESS) {
			// if user choose to stop this process,deny it.
			// we should stop it by user mode.
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			Data->IoStatus.Information = 0;
			retstatus = FLT_PREOP_COMPLETE;
		}
	}
	if (pNameInfo) {
		FltReleaseFileNameInformation(pNameInfo);
	}
	ExFreePoolWithTag(puImgPath, POOL_TAG_IMGNAME);

	return retstatus;
}