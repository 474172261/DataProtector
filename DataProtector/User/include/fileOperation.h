#pragma once

#include <Windows.h>
#include "dataProtectorUser.h"

void CleanBackupFiles(void);

void CleanInfoArray(void);

int CheckIfNeedNotice(UINT32 pid, PIDINFO *outInfo);

void DelFromArrayAndIfRecoveryFiles(UINT32 pid, BOOL needRecovery);

int AddToArrayAndBackupFiles(WCHAR *filepath, WCHAR *imgpath, UINT32 pid);