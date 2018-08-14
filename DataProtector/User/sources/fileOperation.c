#include <Windows.h>
#include "fileOperation.h"
#include "dataProtectorUser.h"
#include "debug.h"
#include "helper.h"


PIDINFOLINK *gInfoArray = NULL;

int BackupFile(WCHAR *filepath, WCHAR *dstPath) {
	int status;

	status = GetTempFileNameW(gwBackupFolder, L"bak", 0, dstPath);// this function will create an empty file.
	if (!status) {
		dprint(LEVEL_ERROR, "Can't get tmp file name:0x%x", GetLastError());
		return FALSE;
	}

	status = CopyFileW(filepath, dstPath, FALSE);// just overwrite it.
	if (!status) {
		dwprint(LEVEL_ERROR, L"Can't copy file.src:%s\ndst:%s\n0x%x", filepath, dstPath, GetLastError());
		return FALSE;
	}
	dwprint(LEVEL_INFO, L"backup as %s", dstPath);
	return TRUE;
}

int RecoveryFiles(PIDINFO *info) {
	int ret;
	BOOL result = TRUE;
	for (int i = 0; i < info->pathCount; i++) {
		ret = MoveFileW(info->path[i].bakPath, info->path[i].orgPath);// just overwrite it.
		if (!ret) {
			dwprint(LEVEL_ERROR, L"Can't recovery file.src:%s\ndst:%s\n0x%x",
				info->path[i].bakPath,
				info->path[i].orgPath,
				GetLastError()
			);
			DeleteFileW(info->path[i].bakPath);// if we can't recovery,delete it.
			result = FALSE;
		}
	}
	return result;
}

int DeleteFiles(PIDINFO *info) {
	int ret;
	BOOL result = TRUE;
	for (int i = 0; i < info->pathCount; i++) {
		dwprint(LEVEL_INFO1, L"Trying to delete %s", info->path[i].bakPath);
		ret = DeleteFileW(info->path[i].bakPath);
		if (!ret) {
			dwprint(LEVEL_ERROR, L"Can't DeleteFileW file.%s\n0x%x",
				info->path[i].bakPath,
				GetLastError()
			);
			result = FALSE;
		}
	}
	return result;
}

int AddToArrayAndBackupFiles(WCHAR *filepath, WCHAR *imgpath, UINT32 pid) {
	// you need to check path size before call this functions
	// you need to check if path count >=5;
	PIDINFOLINK *tmp;
	PIDINFO *info = NULL;
	WCHAR dstPath[MAX_PATH];

	if (!BackupFile(filepath, dstPath)) {
		return FALSE;
	}

	EnterCriticalSection(&g_cs);

	if (gInfoArray) {
		tmp = gInfoArray;
		do {
			if (tmp->info.pid == pid) {
				info = &tmp->info;
				for (int i = 0; i < info->pathCount; i++) {
					if (!_wcsicmp(info->path[i].orgPath, filepath)) {
						// if path exists,don't add it again.
						LeaveCriticalSection(&g_cs);
						return TRUE;
					}
				}
				break;
			}
		} while (tmp = tmp->next);

		if (!info) {
			PIDINFOLINK *link;
			link = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, sizeof(PIDINFOLINK));
			if (!link) {
				dprint(LEVEL_ERROR, "can't alloc memory for link");
				LeaveCriticalSection(&g_cs);
				return FALSE;
			}

			tmp = gInfoArray->next;
			gInfoArray->next = link;
			link->next = tmp;

			info = &link->info;
			info->firstTime = GetTickCount64();
			wcsncpy_s(info->imgPath, MAX_PATH, imgpath, lstrlenW(imgpath));
			info->pid = pid;
		}

	}
	else {
		gInfoArray = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, sizeof(PIDINFOLINK));
		if (!gInfoArray) {
			dprint(LEVEL_ERROR, "can't alloc memory for gInfoArray");
			LeaveCriticalSection(&g_cs);
			return FALSE;
		}
		info = &gInfoArray->info;
		info->firstTime = GetTickCount64();
		wcsncpy_s(info->imgPath, MAX_PATH, imgpath, lstrlenW(imgpath));
		info->pid = pid;
	}
	wcsncpy_s(info->path[info->pathCount].orgPath, MAX_PATH, filepath, lstrlenW(filepath));
	wcsncpy_s(info->path[info->pathCount].bakPath, MAX_PATH, dstPath, lstrlenW(dstPath));

	info->pathCount++;

	LeaveCriticalSection(&g_cs);

	return TRUE;
}


void DelFromArrayAndIfRecoveryFiles(UINT32 pid, BOOL needRecovery) {
	if (gInfoArray) {
		PIDINFOLINK *link = gInfoArray;
		PIDINFOLINK *tmpPre = gInfoArray;

		EnterCriticalSection(&g_cs);

		do {
			if (link->info.pid == pid) {
				tmpPre->next = link->next;
				if (link == gInfoArray) {
					if (link->next == NULL) {
						gInfoArray = NULL;
					}
					else {
						gInfoArray = link->next;
					}
				}
				if (needRecovery) {
					RecoveryFiles(&link->info);
				}
				else {
					DeleteFiles(&link->info);
				}
				HeapFree(gHeap, MEM_RELEASE, link);
				break;
			}
			tmpPre = link;
		} while (link = link->next);

		LeaveCriticalSection(&g_cs);
	}
}

int CheckIfNeedNotice(UINT32 pid, PIDINFO *outInfo) {
	PIDINFOLINK *link = gInfoArray;
	if (gInfoArray) {
		EnterCriticalSection(&g_cs);
		do {
			if (link->info.pid == pid && link->info.pathCount == 5) {
				if (link->isAsking) {
					break;
				}
				link->isAsking = TRUE;// check if asking now.
				memcpy(outInfo, &link->info, sizeof(PIDINFO));
				LeaveCriticalSection(&g_cs);
				return TRUE;
			}
		} while (link = link->next);
		LeaveCriticalSection(&g_cs);
	}
	return FALSE;
}

void RemovePIDfromTempList(UINT32 pid) {
	if (guTempPIDWhiteList) {
		UINT32 count = guTempPIDWhiteList[0];
		for (UINT32 i = 1; i <= count; i++) {
			if (guTempPIDWhiteList[i] == pid) {
				guTempPIDWhiteList[i] = 0;
				guTempPIDWhiteList[0] = count - 1;
			}
		}
	}
}

void CleanBackupFiles(void) {
	PIDINFOLINK *link = gInfoArray;
	PIDINFOLINK *tmpPre = gInfoArray;
	PIDINFOLINK *tmp;
	if (link) {
		EnterCriticalSection(&g_cs);
		do {
			if (!IsProcessAlive(link->info.pid)
				|| (GetTickCount64() - link->info.firstTime) > 1000 * 60
				) 
			{
				RemovePIDfromTempList(link->info.pid);
				DeleteFiles(&link->info);
				if (link == gInfoArray) {
					if (link->next == NULL) {
						gInfoArray = NULL;
						HeapFree(gHeap, MEM_RELEASE, link);
						break;
					}
					else {
						gInfoArray = link->next;
						tmpPre = gInfoArray;

					}
				}
				else {
					tmpPre->next = link->next;
				}
				tmp = link;
				link = link->next;
				HeapFree(gHeap, MEM_RELEASE, tmp);
			}
			else {
				tmpPre = link;
				link = link->next;
			}
		} while (link);
		LeaveCriticalSection(&g_cs);
	}
}

void CleanInfoArray(void) {
	if (gInfoArray) {
		PIDINFOLINK *link = gInfoArray;
		PIDINFOLINK *next = NULL;
		EnterCriticalSection(&g_cs);
		do {
			if (link) {
				next = link->next;
				HeapFree(gHeap, MEM_RELEASE, link);
			}
		} while (link = next);
		LeaveCriticalSection(&g_cs);
	}
}