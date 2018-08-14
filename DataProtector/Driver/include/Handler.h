#pragma once
#include <NTDDk.h>

int FilterWhiteImagePath(UNICODE_STRING *puImagePath);

int FilterWhiteDir(UNICODE_STRING *puFilePath);

int FilterFileTypeAndWhiteList(
	PFLT_CALLBACK_DATA Data,
	UNICODE_STRING *puImgPath,
	PFLT_FILE_NAME_INFORMATION *ppFileNameInfo
);

UINT32 
HandleEvents(
	UNICODE_STRING *puFilePath, 
	UNICODE_STRING *puImgPath, 
	PEPROCESS pEprocess, 
	UINT32 operation
);


FLT_PREOP_CALLBACK_STATUS HandleOperation(
	PFLT_CALLBACK_DATA Data,
	UINT32 operation);