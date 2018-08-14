#pragma once
#include <windows.h>

typedef struct _PIDINFO {
	UINT32 pid;
	ULONGLONG firstTime;
	WCHAR imgPath[MAX_PATH];
	struct {
		WCHAR orgPath[MAX_PATH];
		WCHAR bakPath[MAX_PATH];
	} path[5];
	int pathCount;
} PIDINFO;// 5744

typedef struct _link {
	struct _link *next;
	PIDINFO info;
	BOOL isAsking;
} PIDINFOLINK;

typedef struct {
	ULONGLONG msgID;
	UINT32 operation;
	PIDINFO info;
} PIDPARAMETER;

extern CRITICAL_SECTION g_cs;
extern WCHAR *gwBackupFolder;
extern UINT32 *guTempPIDWhiteList;
extern HANDLE gHeap;

#define DLL_EXPORT_API extern __declspec(dllexport)