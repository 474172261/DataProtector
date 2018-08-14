#include <Windows.h>
#include "debug.h"
#include "dataProtectorUser.h"

int DeviceNameToVolumePathName(WCHAR *filepath) {
	WCHAR devName[100];
	WCHAR fileName[1024];
	HANDLE FindHandle = INVALID_HANDLE_VALUE;
	WCHAR  VolumeName[MAX_PATH];
	DWORD  Error = ERROR_SUCCESS;
	size_t Index = 0;
	DWORD  CharCount = MAX_PATH + 1;

	int index = 0;
	// locate \device\harddiskvolume2\ <-
	for (int i = 0; i < lstrlenW(filepath); i++) {
		if (!memcmp(&filepath[i], L"\\", 2)) {
			index++;
			if (index == 3) {
				index = i;
				break;
			}
		}
	}
	if (index >= 100) {
		dprint(LEVEL_ERROR, "DeviceName is too long!");
		return FALSE;
	}
	filepath[index] = L'\0';

	FindHandle = FindFirstVolumeW(VolumeName, ARRAYSIZE(VolumeName));

	if (FindHandle == INVALID_HANDLE_VALUE)
	{
		Error = GetLastError();
		dprint(LEVEL_ERROR, "FindFirstVolumeW failed with error code 0x%x\n", Error);
		return FALSE;
	}
	for (;;)
	{
		//  Skip the \\?\ prefix and remove the trailing backslash.
		Index = wcslen(VolumeName) - 1;

		if (VolumeName[0] != L'\\' ||
			VolumeName[1] != L'\\' ||
			VolumeName[2] != L'?' ||
			VolumeName[3] != L'\\' ||
			VolumeName[Index] != L'\\')
		{
			Error = ERROR_BAD_PATHNAME;
			dwprint(LEVEL_ERROR, L"FindFirstVolumeW/FindNextVolumeW returned a bad path: %s\n", VolumeName);
			break;
		}
		VolumeName[Index] = L'\0';
		CharCount = QueryDosDeviceW(&VolumeName[4], devName, 100);
		if (CharCount == 0)
		{
			Error = GetLastError();
			dprint(LEVEL_ERROR, "QueryDosDeviceW failed with error code 0x%x\n", Error);
			break;
		}
		if (!lstrcmpW(devName, filepath)) {
			VolumeName[Index] = L'\\';
			Error = GetVolumePathNamesForVolumeNameW(VolumeName, fileName, CharCount, &CharCount);
			if (!Error) {
				Error = GetLastError();
				dprint(LEVEL_ERROR, "GetVolumePathNamesForVolumeNameW failed with error code 0x%x\n", Error);
				break;
			}

			lstrcatW(fileName, &filepath[index + 1]);
			lstrcpyW(filepath, fileName);

			Error = ERROR_SUCCESS;
			break;
		}

		Error = FindNextVolumeW(FindHandle, VolumeName, ARRAYSIZE(VolumeName));

		if (!Error)
		{
			Error = GetLastError();

			if (Error != ERROR_NO_MORE_FILES)
			{
				dprint(LEVEL_ERROR, "FindNextVolumeW failed with error code 0x%x\n", Error);
				break;
			}

			//
			//  Finished iterating
			//  through all the volumes.
			Error = ERROR_BAD_PATHNAME;
			break;
		}
	}

	FindVolumeClose(FindHandle);
	if (Error != ERROR_SUCCESS)
		return FALSE;
	return TRUE;

}

int SilentlyRemoveDirectory(const WCHAR* dir) // delete files include folder.
{
	int len = (lstrlenW(dir) + 2) * sizeof(WCHAR); // required to set 2 nulls at end of argument to SHFileOperation.
	wchar_t* tempdir = (wchar_t*)HeapAlloc(gHeap,
		HEAP_ZERO_MEMORY,
		len);
	memcpy_s(tempdir, len, dir, lstrlenW(dir) * sizeof(WCHAR));

	SHFILEOPSTRUCTW file_op = {
		NULL,
		FO_DELETE,
		tempdir,
		NULL,
		FOF_NOCONFIRMATION |
		FOF_NOERRORUI |
		FOF_SILENT |
		FOF_FILESONLY,
		FALSE,
		0,
		L"" };

	int ret = SHFileOperationW(&file_op);// returns 0 on success, non zero on failure.
	HeapFree(gHeap, MEM_RELEASE, tempdir);

	if (ret)
		return FALSE;
	return TRUE;
}

wchar_t*
FromUTF8toWideChar(
	const char* src,
	int src_length,  /* = 0 */
	int* out_length  /* = NULL */
)
{
	if (!src)
	{
		return NULL;
	}

	if (src_length == 0) { src_length = lstrlenA(src); }
	int length = MultiByteToWideChar(CP_UTF8, 0, src, src_length, NULL, 0);
	if (!length) {
		dprint(LEVEL_ERROR, "Can't convert utf8 to wchar:0x%x", GetLastError());
		return NULL;
	}
	wchar_t *output_buffer = (wchar_t*)HeapAlloc(gHeap,
		HEAP_ZERO_MEMORY,
		(length + 1) * sizeof(wchar_t));

	if (output_buffer) {
		MultiByteToWideChar(CP_UTF8, 0, src, src_length, output_buffer, length);
		output_buffer[length] = L'\0';
	}
	if (out_length) { *out_length = length; }
	return output_buffer;
}

char*
toUTF8(
	const wchar_t* src,
	int src_length,  /* = 0 */
	int* out_length  /* = NULL */
)
{
	if (!src)
	{
		return NULL;
	}

	if (src_length == 0) { src_length = lstrlenW(src); }
	int length = WideCharToMultiByte(CP_UTF8, 0, src, src_length,
		0, 0, NULL, NULL);
	if (!length) {
		dprint(LEVEL_ERROR, "Can't convert wchar to utf8:0x%x", GetLastError());
		return NULL;
	}
	char *output_buffer = (char*)HeapAlloc(
		gHeap,
		HEAP_ZERO_MEMORY,
		(length + 1) * sizeof(char)
	);

	if (output_buffer) {
		WideCharToMultiByte(CP_UTF8, 0, src, src_length,
			output_buffer, length, NULL, NULL);
		output_buffer[length] = '\0';
	}
	if (out_length) { *out_length = length; }
	return output_buffer;
}

int GetCurrentPath(WCHAR *path, int maxsize) {
	if (!GetModuleFileNameW(NULL, path, maxsize)) {
		dprint(LEVEL_ERROR, "Can't get current directory:0x%x", GetLastError());
		return FALSE;
	}

	for (int index = lstrlenW(path) - 1; index; index--) {
		if (path[index] == L'\\') {
			path[index] = L'\0';
			break;
		}
	}
	return TRUE;
}


int IsProcessAlive(UINT32 pid) {
	HANDLE ret = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (!ret) {
		UINT32 error = GetLastError();
		if(error != ERROR_INVALID_PARAMETER)
			dprint(LEVEL_ERROR, "OpenProcess fail.0x%x\n", GetLastError());
		return FALSE;
	}
	DWORD code;
	BOOL res = GetExitCodeProcess(ret, &code);
	if (!res) {
		dprint(LEVEL_ERROR, "GetExitCodeProcess fail,0x%x\n", GetLastError());
		return FALSE;
	}
	CloseHandle(ret);
	if (code == STILL_ACTIVE) {
		return TRUE;
	}
	return FALSE;
}

int Stricmp(WCHAR *target, WCHAR *src) {
	int len = lstrlenW(src);
	int max = lstrlenW(target);
	if (len > max) {
		WCHAR tmp = src[max];
		src[max] = L'\0';
		if (!_wcsicmp(target, src)) {
			src[max] = tmp;
			return TRUE;
		}
		src[max] = tmp;
	}
	return FALSE;
}