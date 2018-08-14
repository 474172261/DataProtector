#include "debug.h"
#include "helper.h"
#include "dataprotectorUser.h"

#define DEBUGLOG L"[DBGLOG] "

#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>

void SaveToFileByUTF8(void *buf, int n, int UTF8) {
	HANDLE hFile;
	hFile = CreateFileW(L"D:\\test\\log.txt",
		FILE_APPEND_DATA,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("can't open log.txt,%x", GetLastError());
	}
	else {
		int len;
		if (UTF8) {
			char *data = toUTF8(buf, n, &len);// remember to free it.
			if (data) {
				WriteFile(hFile, data, len, &len, NULL);
				WriteFile(hFile, "\n", 1, &len, NULL);
				HeapFree(gHeap, MEM_RELEASE, data);
			}
		}
		else {
			WriteFile(hFile, buf, n, &len, NULL);
		}
		CloseHandle(hFile);
	}
}

#ifndef DEBUG_PRINT

int dprint(const char *fmt, ...) {
	SYSTEMTIME lt;
	char buf[0x200];
	va_list args;
	int n;
	va_start(args, fmt);
	n = vsnprintf(buf, 0x200, fmt, args);
	if (!Log_Path) {
		return 0;
	}
	FILE *fp;
	if (_wfopen_s(&fp, Log_Path, L"a")) {
		OutputDebugStringW(DEBUGLOG L"Log path wrong\n");
		return 0;
	}
	GetLocalTime(&lt);
	fprintf_s(fp, "%d-%d,%d:%d. [PID]%d, %s\n", lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, GetCurrentProcessId(), buf);
	fclose(fp);
	va_end(args);
	return n;
}

int dfprint(FILE *stream, const char *fmt, ...) {
	SYSTEMTIME lt;
	char buf[0x200];
	va_list args;
	int n;
	va_start(args, fmt);
	n = vsnprintf(buf, 0x200, fmt, args);
	if (!Log_Path) {
		return 0;
	}
	FILE *fp;
	if (_wfopen_s(&fp, Log_Path, L"a")) {
		OutputDebugStringW(DEBUGLOG L"Log path wrong\n");
		return 0;
	}
	GetLocalTime(&lt);
	fprintf_s(fp, "%d-%d,%d:%d. [PID]%d, %s\n", lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, GetCurrentProcessId(), buf);
	fclose(fp);
	va_end(args);
	return n;
}

#else

int dprint(int level, const char *fmt, ...) {
	if (level < DEBUG_LEVEL)
		return 0;
	char buf[0x200];
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsnprintf(buf, 0x200, fmt, args);
	printf("%s\n", buf);
	SaveToFileByUTF8(buf, n, FALSE);
	va_end(args);
	return n;
}

int dwprint(int level, const wchar_t *fmt, ...) {
	if (level < DEBUG_LEVEL)
		return 0;
	wchar_t buf[0x200];
	va_list args;
	int n;

	va_start(args, fmt);
	n = _vsnwprintf_s(buf, 0x200, 0x200, fmt, args);
	wprintf(L"%s\n", buf);
	SaveToFileByUTF8(buf, n, TRUE);
	va_end(args);
	return n;
}

#endif