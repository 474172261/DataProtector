#include <Shlobj.h>
#include "dataProtectorUser.h"
#include "debug.h"
#include "helper.h"


//#pragma warning(disable:4996)
#pragma comment(lib,"Shell32.lib")

WCHAR **gwppWhiteDirs = NULL;
WCHAR **gwppWhiteList = NULL;
UINT32 gWhiteListCount = 0;

int InitWhiteList() {
	WCHAR *buffer = NULL;
	UINT8 *tmpBuffer = NULL;
	WCHAR currentPath[MAX_PATH + 1];
	HANDLE file = INVALID_HANDLE_VALUE;
	DWORD count = 0;
	DWORD fileSize;
	DWORD index;

	// add whitedir, remember to change uninitWhitelist if you change here.
	gwppWhiteDirs = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, 3 * sizeof(WCHAR*));
	if (!gwppWhiteDirs) {
		dprint(LEVEL_ERROR, "Can't alloc heap for gwppWhiteDirs");
		return FALSE;
	}

	WCHAR *usrProfilePath = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, (MAX_PATH + 1) * sizeof(WCHAR));
	if (!usrProfilePath) {
		dprint(LEVEL_ERROR, "Can't alloc heap for usrProfilePath");
		return FALSE;
	}

	HRESULT ret = SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, usrProfilePath);
	if (S_OK != ret) {
		dprint(LEVEL_ERROR, "Can't get userprofile folder,0x%x", ret);
		return FALSE;
	}
	lstrcatW(usrProfilePath, L"\\AppData\\");
	gwppWhiteDirs[0] = usrProfilePath;
	gwppWhiteDirs[1] = L"C:\\windows\\";
	/*
	the path have to end with \\,avoid folder name like AppData???
	*/

	gwppWhiteList = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, 2 * sizeof(WCHAR*));
	if (!gwppWhiteList) {
		dprint(LEVEL_ERROR, "can't alloc heap for gwppWhiteList");
		return -1;
	}
	WCHAR *cur_exe = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, (MAX_PATH + 1) * sizeof(WCHAR));
	if (!GetModuleFileNameW(NULL, cur_exe, MAX_PATH)) {
		dprint(LEVEL_ERROR, "Can't get exe path:0x%x", GetLastError());
		return -1;
	}
	gwppWhiteList[0] = cur_exe;
	gWhiteListCount = 1;// one for explorer.one for null end.

	if (!GetCurrentPath(currentPath, MAX_PATH)) {
		return FALSE;
	}

	lstrcatW(currentPath, L"\\whitelist.txt");
	dwprint(LEVEL_INFO1, L"%s", currentPath);
	file = CreateFileW(currentPath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (file == INVALID_HANDLE_VALUE) {
		dprint(LEVEL_ERROR, "can't open whitelist:0x%x", GetLastError());
		return FALSE;
	}

	fileSize = GetFileSize(file, NULL);
	if (fileSize == INVALID_FILE_SIZE) {
		dprint(LEVEL_ERROR, "can't get whitelist size:0x%x", GetLastError());
		CloseHandle(file);
		return FALSE;
	}
	if (!fileSize) {
		dprint(LEVEL_ERROR, "empty file");
		CloseHandle(file);
		return FALSE;
	}

	tmpBuffer = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, fileSize + 1);
	if (!tmpBuffer) {
		dprint(LEVEL_ERROR, "alloc buffer for whitelist");
		CloseHandle(file);
		return FALSE;
	}
	if (!ReadFile(file, tmpBuffer, fileSize, &fileSize, NULL)) {
		dprint(LEVEL_ERROR, "read file fail:0x%x", GetLastError());
		CloseHandle(file);
		HeapFree(gHeap, MEM_RELEASE, tmpBuffer);
		return FALSE;
	}
	CloseHandle(file);
	tmpBuffer[fileSize] = '\0';

	if (fileSize > 3 && tmpBuffer[0] == 0xEF && tmpBuffer[1] == 0xBB && tmpBuffer[2] == 0xBF) {
		buffer = FromUTF8toWideChar(tmpBuffer + 3, fileSize - 3, &fileSize);
	}
	else {
		buffer = FromUTF8toWideChar(tmpBuffer, fileSize, &fileSize);
	}

	HeapFree(gHeap, MEM_RELEASE, tmpBuffer);

	if (!buffer) {
		return FALSE;
	}
	// count for path,path must end with '|'.
	for (index = 0; index < fileSize; index++) {
		if (buffer[index] == L'|') {
			count++;
		}
	}

	if (!count) {// avoid someone changed file.
		HeapFree(gHeap, MEM_RELEASE, buffer);
		return TRUE;
	}

	gwppWhiteList = HeapReAlloc(gHeap,
		HEAP_ZERO_MEMORY,
		gwppWhiteList,
		(count + gWhiteListCount + 2) * sizeof(WCHAR*)
	);// avoid path not end with '|',alloc more 2 as end.

	if (!gwppWhiteList) {
		dprint(LEVEL_ERROR, "can't alloc heap for gwppWhiteList");
		HeapFree(gHeap, MEM_RELEASE, buffer);
		return -1;
	}
	gwppWhiteList[1] = buffer;// reuse this heap
	count = 1;// recount again

	for (index = 0; index < fileSize; index++) {
		if (buffer[index] == L'|' && index + 1 != fileSize) {
			gwppWhiteList[++count] = &buffer[index + 1];
			buffer[index] = L'\0';
		}
	}
	buffer[fileSize - 1] = L'\0';
	gWhiteListCount = count + 1;

	return TRUE;
}

int AddNewtoWhiteList(WCHAR *path, UINT32 pid, BOOL saveToFile) {
	HANDLE hFile;
	WCHAR wcWhiteListPath[MAX_PATH];
	DWORD dwPos, dwBytesWritten;
	WCHAR *newpath;
	char *tmpPath;
	DWORD pathLen = lstrlenW(path);

	if (gwppWhiteList) {
		if (saveToFile) {

			gwppWhiteList = HeapReAlloc(gHeap,
				HEAP_ZERO_MEMORY,
				gwppWhiteList,
				(gWhiteListCount + 2) * sizeof(WCHAR*)
			);
			if (!gwppWhiteList) {
				dprint(LEVEL_ERROR, "can't alloc heap for new gwppWhiteList");
				return -1;
			}

			newpath = HeapAlloc(gHeap, HEAP_ZERO_MEMORY, (MAX_PATH + 1) * sizeof(WCHAR));
			if (!newpath) {
				dprint(LEVEL_ERROR, "alloc for new path fail");
				return FALSE;
			}

			wcsncpy_s(newpath, MAX_PATH, path, pathLen);
			gwppWhiteList[gWhiteListCount] = newpath;
			gWhiteListCount++;

			if (GetCurrentPath(wcWhiteListPath, MAX_PATH)) {
				wcsncat_s(wcWhiteListPath, MAX_PATH, L"\\whitelist.txt", 14);
				dwprint(LEVEL_INFO1, L"Add white:%s", wcWhiteListPath);
				hFile = CreateFileW(wcWhiteListPath,
					FILE_APPEND_DATA,
					FILE_SHARE_READ,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
				if (hFile == INVALID_HANDLE_VALUE) {
					dprint(LEVEL_ERROR, "can't open whitelist,%x", GetLastError());
				}
				else {
					dwPos = SetFilePointer(hFile, 0, NULL, FILE_END);
					tmpPath = toUTF8(path, 0, &pathLen);// remember to free it.
					if (tmpPath) {
						LockFile(hFile, dwPos, 0, pathLen + 1, 0);
						WriteFile(hFile, tmpPath, pathLen, &dwBytesWritten, NULL);
						WriteFile(hFile, "|", 1, &dwBytesWritten, NULL);
						UnlockFile(hFile, dwPos, 0, pathLen + 1, 0);

						HeapFree(gHeap, MEM_RELEASE, tmpPath);
					}
					CloseHandle(hFile);
				}
			}
		}
		else {
			UINT32 count = guTempPIDWhiteList[0];
			if (count && !(count % 32)) {
				guTempPIDWhiteList = HeapReAlloc(gHeap,
					HEAP_ZERO_MEMORY,
					guTempPIDWhiteList,
					32 * (1 + (count / 32)) * sizeof(UINT32));
				if (guTempPIDWhiteList) {
					dprint(LEVEL_ERROR, "Can't realloc guTempPIDWhiteList");
					return FALSE;
				}
			}
			if (!count) {
				guTempPIDWhiteList[1] = pid;
				guTempPIDWhiteList[0] = 1;
				dprint(LEVEL_INFO1, "Add tmp pid:%d", pid);
			}
			for (UINT32 i = 1; i <= count; i++) {
				if (guTempPIDWhiteList[i] == 0) {// add tmp to not used
					guTempPIDWhiteList[i] = pid;
					guTempPIDWhiteList[0] = count + 1;
					dprint(LEVEL_INFO1, "Add tmp pid:%d", pid);
					break;
				}
			}

		}

		return TRUE;
	}
	return FALSE;
}

int InWhiteList(WCHAR *filepath, WCHAR *imgPath, UINT32 pid) {

	if (gwppWhiteList) {
		for (UINT32 i = 0; i < gWhiteListCount; i++) {
			if (!_wcsicmp(imgPath, gwppWhiteList[i])) {
				dprint(LEVEL_INFO, "This is in whitelist!");
				return TRUE;
			}
		}
	}

	if (gwppWhiteDirs) {
		for (int i = 0; gwppWhiteDirs[i]; i++) {
			if (Stricmp(gwppWhiteDirs[i], filepath)) {
				dprint(LEVEL_INFO, "This is in whiteDirs!");
				return TRUE;
			}
		}
	}

	if (guTempPIDWhiteList) {
		UINT32 count = guTempPIDWhiteList[0];
		for (UINT32 i = 1; i <= count; i++) {
			dprint(LEVEL_INFO, "now tmp pid:%d", guTempPIDWhiteList[i]);
			if (guTempPIDWhiteList[i] == pid) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

void UninitWhiteList() {
	if (gwppWhiteList) {
		if (gwppWhiteDirs[0]) {
			HeapFree(gHeap, MEM_RELEASE, gwppWhiteList[0]);
		}
		if (gwppWhiteList[1]) {
			HeapFree(gHeap, MEM_RELEASE, gwppWhiteList[1]);
		}
		HeapFree(gHeap, MEM_RELEASE, gwppWhiteList);
	}
	if (gwppWhiteDirs) {
		if (gwppWhiteDirs[0]) {
			HeapFree(gHeap, MEM_RELEASE, gwppWhiteDirs[0]);
		}
		HeapFree(gHeap, MEM_RELEASE, gwppWhiteDirs);
	}
}