#include <Windows.h>
#include <fltuser.h>
#include "debug.h"
#include "communication.h"
#include "dataProtectorUser.h"
#include "helper.h"
#include "fileOperation.h"
#include "whitelist.h"
#include "Queue.h"

#pragma comment(lib,"FltLib.lib")
#pragma comment(lib,"Shell32.lib")

#define BACKUP_FILE_FOLDER L"\\Backup"
#define GUARD_RANSOMWARE_PORT_NAME L"\\MiniFilter_Guard_Ransomware"

WCHAR *gwBackupFolder = NULL;
UINT32 *guTempPIDWhiteList = NULL;// the first used as count
HANDLE ghPort = NULL;
HANDLE gHeap = NULL;

CRITICAL_SECTION g_cs;

BOOL gContinue = TRUE;

#ifndef _Dll
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal. 
	case CTRL_C_EVENT:
		printf("Ctrl-C event\n\n");
		gContinue = FALSE;
		return(TRUE);

		// CTRL-CLOSE: confirm that the user wants to exit. 
	case CTRL_CLOSE_EVENT:
		gContinue = FALSE;
		printf("Ctrl-Close event\n\n");
		return(TRUE);

		// Pass other signals to the next handler. 
	//case CTRL_BREAK_EVENT:
	//	gContinue = FALSE;
	//	printf("Ctrl-Break event\n\n");
	//	return FALSE;

	case CTRL_LOGOFF_EVENT:
		gContinue = FALSE;
		printf("Ctrl-Logoff event\n\n");
		return FALSE;

	case CTRL_SHUTDOWN_EVENT:
		gContinue = FALSE;
		printf("Ctrl-Shutdown event\n\n");
		return FALSE;

	default:
		return FALSE;
	}
}
#else

typedef UINT32(WINAPI *PyCallBack)(PIDINFO *info);
PyCallBack gPyCallBack = NULL;


DLL_EXPORT_API void WINAPI StopDataProtector(void) {
	gContinue = FALSE;
}

#endif //_Dll


void AskForUserToHandle(PIDPARAMETER *parameter) {
	int ret;
	REPLY_STRUCT reply;
	PIDINFO *info = &parameter->info;
	UINT32 size = sizeof(WCHAR) * MAX_PATH * 7;
	WCHAR *buffer = HeapAlloc(gHeap,
		HEAP_ZERO_MEMORY,
		size);
#ifdef _Dll
	if (gPyCallBack) {
		gPyCallBack(info);
	}
#endif //_Dll
	if (parameter->operation == HANDLE_OPERATION_OVERWRITE) {

		swprintf(buffer, size,
			L"pid:%d\n"
			L"path: %s\n"
			L"List:\n"
			L"  %s\n  %s\n  %s\n  %s\n  %s\n"
			L"This program is trying to overwrite many files on your computer.\n"
			L"Do you want to stop it?",// 167 bytes.
			info->pid,
			info->imgPath,
			info->path[0].orgPath,
			info->path[1].orgPath,
			info->path[2].orgPath,
			info->path[3].orgPath,
			info->path[4].orgPath
		);
	}
	else if (parameter->operation == HANDLE_OPERATION_DELETE) {

		swprintf(buffer, size,
			L"pid:%d\n"
			L"path: %s\n"
			L"This program is trying to delete many files from your computer.\n"
			L"List:\n"
			L"  %s\n  %s\n  %s\n  %s\n  %s\n"
			L"Do you want to stop it?",// 167 bytes.
			info->pid,
			info->imgPath,
			info->path[0].orgPath,
			info->path[1].orgPath,
			info->path[2].orgPath,
			info->path[3].orgPath,
			info->path[4].orgPath
		);
	}

	ret = MessageBoxW(NULL, buffer, L"Warning", MB_YESNO | MB_ICONWARNING | MB_TOPMOST);

	HeapFree(gHeap, MEM_RELEASE, buffer);
	reply.Data.code = REPLY_CODE_NONE;

	if (ret == IDYES) {
		reply.Data.code = REPLY_CODE_STOPPROCESS;
		DelFromArrayAndIfRecoveryFiles(info->pid, TRUE);
	}
	else {// user allowed it
		if (MessageBoxW(NULL, L"Do you want to allow it forever?",
			L"Notice",
			MB_YESNO | MB_ICONWARNING | MB_TOPMOST
		)
			== IDYES)
		{
			AddNewtoWhiteList(info->imgPath, 0, TRUE);
		}
		else {
			AddNewtoWhiteList(info->imgPath, info->pid, FALSE);
		}

		DelFromArrayAndIfRecoveryFiles(info->pid, FALSE);
	}

	reply.Header.Status = 0;
	reply.Header.MessageId = parameter->msgID;

	dprint(LEVEL_INFO, "**************asker reply MSGid:%lld********", parameter->msgID);

	ret = FilterReplyMessage(ghPort, (PFILTER_REPLY_HEADER)&reply, sizeof(REPLY_STRUCT));

	if (ret != S_OK) {
		if (ret == ERROR_FLT_NO_WAITER_FOR_REPLY) {
			dprint(LEVEL_INFO1,
				"Timeout!\n pid:%d,%s \nStop it in default.",
				info->pid,
				info->imgPath);
		}
		else {
			dprint(LEVEL_ERROR, "Thread can't reply message,error:0x%x", ret);
		}
	}
}

DWORD WINAPI EventHandlerForAsk(VOID *lpParameter) {
	UNREFERENCED_PARAMETER(lpParameter);
	PIDPARAMETER *info;
	while (gContinue) {
		info = Pop();
		if (info) {
			AskForUserToHandle(info);
		}
		Sleep(1);
	}
	dprint(LEVEL_INFO1, "Asker quit!");
	return 0;
}

int InitializeAll() {
	gwBackupFolder = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, (MAX_PATH + 1) * sizeof(WCHAR));
	if (!gwBackupFolder) {
		dprint(LEVEL_ERROR, "Can't alloc heap for gwBackupFolder");
		return FALSE;
	}

	if (!GetCurrentPath(gwBackupFolder, MAX_PATH - sizeof(BACKUP_FILE_FOLDER)/2 - 15)) {
		// 15 is for filename like L"bak02E0.tmp"
		dprint(LEVEL_ERROR, "Current path is too depth to generate tmp file.0x%x",GetLastError());
		return FALSE;
	}

	lstrcatW(gwBackupFolder, BACKUP_FILE_FOLDER);
	// this function won't delete target linked folder or file.
	SilentlyRemoveDirectory(gwBackupFolder);// clear backuped files

	if (!CreateDirectoryW(gwBackupFolder, NULL)) {
		int error = GetLastError();
		if (error != ERROR_ALREADY_EXISTS) {
			dprint(LEVEL_ERROR, "Can't create backupfolder:0x%x", error);
			return FALSE;
		}
	}

	if (InitWhiteList() == -1) {// -1 means memory can't alloc.
		dprint(LEVEL_ERROR, "Can't init Whitelist");
		return FALSE;
	}

	guTempPIDWhiteList = HeapAlloc(gHeap,
		HEAP_ZERO_MEMORY,
		32 * sizeof(UINT32));

	if (!guTempPIDWhiteList) {
		dprint(LEVEL_ERROR, "Can't alloc for guTempPIDWhiteList");
		return FALSE;
	}

	if (!InitQueue()) {
		return FALSE;
	}
	
	InitializeCriticalSection(&g_cs);
	return TRUE;
}

void UninitializeAll() {// clear all backup files if we exit.
	gContinue = FALSE;
	SilentlyRemoveDirectory(gwBackupFolder);
	
	if(gwBackupFolder)
		HeapFree(gHeap, MEM_RELEASE, gwBackupFolder);

	if (guTempPIDWhiteList) {
		HeapFree(gHeap, MEM_RELEASE, guTempPIDWhiteList);
	}

	CleanInfoArray();

	UninitWhiteList();
	UninitQueue();

	DeleteCriticalSection(&g_cs);
}

DWORD WINAPI AutoCleaner(LPVOID lpParameter) {
	UNREFERENCED_PARAMETER(lpParameter);

	ULONGLONG timeTick = GetTickCount64();
	while (gContinue) {
		if (GetTickCount64() > timeTick + 1000 * 30) {
			dprint(LEVEL_INFO, "Clear");
			CleanBackupFiles();
			timeTick = GetTickCount64();
			dprint(LEVEL_INFO, "Clear Exit");
		}
		Sleep(1000);
	}
	dprint(LEVEL_INFO1, "AutoCleaner Exit!");
	return 0;
}


void EventHandler(SENDMSG_STRUCT *pMsg) {
	REPLY_STRUCT reply;
	int ret;
	UINT32 pid;
	PIDPARAMETER parameter;

	dprint(LEVEL_INFO, "---------------Enter check-----------------");

	if (!DeviceNameToVolumePathName(pMsg->Data.imgName)) {
		dprint(LEVEL_ERROR, "Can't convert img path");
	}


	reply.Data.code = REPLY_CODE_NONE;
	pid = pMsg->Data.pid;
	if (!DeviceNameToVolumePathName(pMsg->Data.fileName)) {
		dprint(LEVEL_ERROR, "Can't convert file path");
		dwprint(LEVEL_ERROR, L"Message:\nfilepath:%s\nimageName:%s\npid:%d",
			pMsg->Data.fileName,
			pMsg->Data.imgName,
			pid);
	}
	else {
		dwprint(LEVEL_INFO1, L"filepath:%s\nimageName:%s\npid:%d,operation:%d",
			pMsg->Data.fileName,
			pMsg->Data.imgName,
			pid,
			pMsg->Data.operation);
		if (lstrlenW(pMsg->Data.fileName) >= MAX_PATH
			|| lstrlenW(pMsg->Data.imgName) >= MAX_PATH)
		{
			dprint(LEVEL_ERROR, "File path is too long to handle");

		}
		else {

			if (!InWhiteList(pMsg->Data.fileName, pMsg->Data.imgName, pid)) {//default ignore explorer.exe
				if (CheckIfNeedNotice(pid, &parameter.info)) {
					parameter.msgID = pMsg->MessageHeader.MessageId;
					parameter.operation = pMsg->Data.operation;
					if (!Push(&parameter)) {// push to ask queue
						dwprint(LEVEL_ERROR, 
							L"Can't push into queue.pid:%d,imgpath:%s",
							parameter.info.pid,
							parameter.info.imgPath
						);
						reply.Data.code = REPLY_CODE_STOPPROCESS;
						goto L_reply;
					}
					dprint(LEVEL_INFO1, "***********send to asker***************");
					goto L_noreply;
				}
				else {
					AddToArrayAndBackupFiles(pMsg->Data.fileName,
						pMsg->Data.imgName,
						pid);
				}
			}
		}
	}
L_reply:
	reply.Header.Status = 0;
	reply.Header.MessageId = pMsg->MessageHeader.MessageId;
	dprint(LEVEL_INFO, "reply MSGid:%lld", pMsg->MessageHeader.MessageId);
	ret = FilterReplyMessage(ghPort, (PFILTER_REPLY_HEADER)&reply, sizeof(REPLY_STRUCT));

	if (ret != S_OK) {
		dprint(LEVEL_ERROR, "can't reply message,error:0x%x", ret);
	}
L_noreply:
	dprint(LEVEL_INFO, "---------------Exit  check-----------------");
}

#ifdef _Dll
DLL_EXPORT_API void WINAPI StartDataProtector(PyCallBack callBack) {
#else
void main() {
#endif // _Dll
	SENDMSG_STRUCT *pMsg = NULL;
	OVERLAPPED oOverlap;
	OVERLAPPED *pOverlap;
	DWORD outSize;
	HRESULT ret;
	ULONG_PTR key;
	HANDLE hCompletion = NULL;
	HANDLE hThreadAsker = NULL;
	HANDLE hThreadCleaner = NULL;

	int waiting = FALSE;

#ifdef _Dll
	if (!callBack) {
		dprint(LEVEL_INFO1, "No callback setting!");
	}
	gPyCallBack = callBack;
#endif //_Dll

	gHeap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
	if (!gHeap) {
		dprint(LEVEL_ERROR, "Can't create Heap,0x%x", GetLastError());
		return;
	}

	if (!InitializeAll()) {
		UninitializeAll();
		return;
	}
#ifndef _Dll
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE)) {
		dprint(LEVEL_ERROR, "Can't register ctrlHandler:0x%x", GetLastError());
		UninitializeAll();
		return;
	}
#endif //_Dll

	hThreadCleaner = CreateThread(NULL, 0, AutoCleaner, NULL, 0, NULL);
	if (!hThreadCleaner) {
		dprint(LEVEL_ERROR, "Can't create cleaner,0x%d", GetLastError());
		goto CLEAN;
	}

	hThreadAsker = CreateThread(NULL, 0, EventHandlerForAsk, NULL, 0, NULL);
	if (!hThreadAsker) {
		dprint(LEVEL_ERROR, "Can't create asker,0x%d", GetLastError());
		goto CLEAN;
	}


	ret = FilterConnectCommunicationPort(GUARD_RANSOMWARE_PORT_NAME,
		0,// FLT_PORT_FLAG_SYNC_HANDLE enabled after win8.1,for synchronous.
		NULL,// Pointer to caller-supplied context information
		0,// sizeof Context
		NULL,// security attribute
		&ghPort);

	if (ret != S_OK) {
		if (ret == 0x80070005) {
			dprint(LEVEL_ERROR, "You must run this as admin!");
		}
		else {
			dprint(LEVEL_ERROR, "Can't connect to port,error:0x%x", ret);
		}
		gContinue = FALSE;
		goto CLEAN;
	}

	dprint(LEVEL_INFO1, "Connect Success!\n");
	pMsg = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, sizeof(SENDMSG_STRUCT));
	
	hCompletion = CreateIoCompletionPort(ghPort,
		NULL,
		0,
		1);// thread count

	if (!hCompletion) {
		dprint(LEVEL_ERROR, "Can't create hCompletion,0x%x", GetLastError());
		goto CLEAN;
	}

	while (gContinue) {
		if (!waiting) {
			memset(&oOverlap, 0, sizeof(OVERLAPPED));
			//oOverlap.hEvent = NULL;
			ret = FilterGetMessage(ghPort, (PFILTER_MESSAGE_HEADER)pMsg, sizeof(SENDMSG_STRUCT), &oOverlap);
			if (ret != HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {
				dprint(LEVEL_ERROR, "Get Message Error:0x%x", ret);
				gContinue = FALSE;
				break;
			}
			waiting = TRUE;
		}
		ret = GetQueuedCompletionStatus(hCompletion, &outSize, &key, &pOverlap, 1);

		if (ret) {
			waiting = FALSE;
			EventHandler(pMsg);
		}
		else {
			ret = GetLastError();
			if (ret != WAIT_TIMEOUT) {
				if (ret == ERROR_INVALID_HANDLE) {
					gContinue = FALSE;
				}
				else {
					dprint(LEVEL_ERROR, "WARNING!!! get overlap error:%x", ret);
					dprint(LEVEL_ERROR, "-------------------------------\n");
				}
			}
		}
	}
CLEAN:
	if (pMsg) {
		HeapFree(gHeap, MEM_RELEASE, pMsg);
	}
	if (ghPort != INVALID_HANDLE_VALUE) {
		CloseHandle(ghPort);
	}
	if (hCompletion) {
		CloseHandle(hCompletion);
	}
	if (hThreadCleaner) {
		dprint(LEVEL_INFO1, "Waiting cleaner exit");
		WaitForSingleObject(hThreadCleaner, INFINITE);
		CloseHandle(hThreadCleaner);
	}
	if (hThreadAsker) {
		dprint(LEVEL_INFO1, "Waiting asker exit");
		WaitForSingleObject(hThreadAsker, INFINITE);
		CloseHandle(hThreadAsker);
	}
	UninitializeAll();// wait threads exit before uninit all.


	HeapDestroy(gHeap);// always at last.
	dprint(LEVEL_INFO1, "Cleared and Exit!");
#ifndef _Dll
#ifdef DEBUG_PRINT
	dprint(LEVEL_INFO1, "Press any key to exit.");
	getchar();
#endif
#endif
}

int __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	return TRUE;
}