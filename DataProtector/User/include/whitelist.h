#pragma once

#include <Windows.h>

int InitWhiteList();
int AddNewtoWhiteList(WCHAR *path, UINT32 pid, BOOL saveToFile);
int InWhiteList(WCHAR *filepath, WCHAR *imgPath, UINT32 pid);
void UninitWhiteList();