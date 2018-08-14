#include <Windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <conio.h>
#include <winioctl.h>

#pragma warning(disable:4996)


BOOL InstallDriver(const char* lpszDriverName, const char* lpszDriverPath, const char* lpszAltitude);

BOOL StartDriver(const char* lpszDriverName);

BOOL StopDriver(const char* lpszDriverName);

BOOL DeleteDriver(const char* lpszDriverName);


void Dprint(char *fmt, ...) {
	char buf[0x200];
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsnprintf(buf, 0x200, fmt, args);
	printf("%s\n", buf);
	va_end(args);
}

//======================================== 动态加载/卸载sys驱动 ======================================
// SYS文件跟程序放在同个目录下
// 如果产生的SYS名为HelloDDK.sys,那么安装驱动InstallDriver("HelloDDK",".\\HelloDDK.sys","370030"/*Altitude*/);
// 启动驱动服务 StartDriver("HelloDDK");
// 停止驱动服务 StopDriver("HelloDDK");
// 卸载SYS也是类似的调用过程， DeleteDriver("HelloDDK");
//====================================================================================================

BOOL InstallDriver(const char* lpszDriverName, const char* lpszDriverPath, const char* lpszAltitude)
{
	char    szTempStr[MAX_PATH];
	HKEY    hKey;
	DWORD    dwData;
	char    szDriverImagePath[MAX_PATH];

	if (NULL == lpszDriverName || NULL == lpszDriverPath)
	{
		Dprint("no name or path");
		return FALSE;
	}
	//得到完整的驱动路径
	GetFullPathName(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);

	SC_HANDLE hServiceMgr = NULL;// SCM管理器的句柄
	SC_HANDLE hService = NULL;// NT驱动程序的服务句柄

							  //打开服务控制管理器
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		// OpenSCManager失败
		Dprint("fail open service:%d", GetLastError());
		CloseServiceHandle(hServiceMgr);
		return FALSE;
	}

	// OpenSCManager成功  

	//创建驱动所对应的服务
	hService = CreateService(hServiceMgr,
		lpszDriverName,             // 驱动程序的在注册表中的名字
		lpszDriverName,             // 注册表驱动程序的DisplayName 值
		SERVICE_ALL_ACCESS,         // 加载驱动程序的访问权限
		SERVICE_FILE_SYSTEM_DRIVER, // 表示加载的服务是文件系统驱动程序
		SERVICE_DEMAND_START,       // 注册表驱动程序的Start 值
		SERVICE_ERROR_IGNORE,       // 注册表驱动程序的ErrorControl 值
		szDriverImagePath,          // 注册表驱动程序的ImagePath 值
		"FSFilter Activity Monitor",// 注册表驱动程序的Group 值
		NULL,
		"FltMgr",                   // 注册表驱动程序的DependOnService 值
		NULL,
		NULL);

	if (hService == NULL)
	{
		if (GetLastError() == ERROR_SERVICE_EXISTS)
		{
			//服务创建失败，是由于服务已经创立过
			CloseServiceHandle(hService);       // 服务句柄
			CloseServiceHandle(hServiceMgr);    // SCM句柄
			Dprint("already exists.");
			return TRUE;
		}
		else
		{
			CloseServiceHandle(hService);       // 服务句柄
			CloseServiceHandle(hServiceMgr);    // SCM句柄
			Dprint("fail create service:%d", GetLastError());
			return FALSE;
		}
	}
	CloseServiceHandle(hService);       // 服务句柄
	CloseServiceHandle(hServiceMgr);    // SCM句柄

										//-------------------------------------------------------------------------------------------------------
										// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances子健下的键值项 
										//-------------------------------------------------------------------------------------------------------
	strcpy(szTempStr, "SYSTEM\\CurrentControlSet\\Services\\");
	strcat(szTempStr, lpszDriverName);
	strcat(szTempStr, "\\Instances");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, "", TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		Dprint("fail create key Instances:%d", GetLastError());
		return FALSE;
	}
	// 注册表驱动程序的DefaultInstance 值 
	strcpy(szTempStr, lpszDriverName);
	strcat(szTempStr, " Instance");
	if (RegSetValueEx(hKey, "DefaultInstance", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)strlen(szTempStr)) != ERROR_SUCCESS)
	{
		Dprint("fail set DefaultInstance:%d", GetLastError());
		return FALSE;
	}
	RegFlushKey(hKey);//刷新注册表
	RegCloseKey(hKey);
	//-------------------------------------------------------------------------------------------------------

	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance子健下的键值项 
	//-------------------------------------------------------------------------------------------------------
	strcpy(szTempStr, "SYSTEM\\CurrentControlSet\\Services\\");
	strcat(szTempStr, lpszDriverName);
	strcat(szTempStr, "\\Instances\\");
	strcat(szTempStr, lpszDriverName);
	strcat(szTempStr, " Instance");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, "", TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		Dprint("fail create key Instance:%d", GetLastError());
		return FALSE;
	}
	// 注册表驱动程序的Altitude 值
	strcpy(szTempStr, lpszAltitude);
	if (RegSetValueEx(hKey, "Altitude", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)strlen(szTempStr)) != ERROR_SUCCESS)
	{
		Dprint("fail create value altitude:%d", GetLastError());
		return FALSE;
	}
	// 注册表驱动程序的Flags 值
	dwData = 0x0;
	if (RegSetValueEx(hKey, "Flags", 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		Dprint("fail create value flags:%d", GetLastError());
		return FALSE;
	}
	RegFlushKey(hKey);//刷新注册表
	RegCloseKey(hKey);
	//-------------------------------------------------------------------------------------------------------

	return TRUE;
}

BOOL StartDriver(const char* lpszDriverName)
{
	SC_HANDLE        schManager;
	SC_HANDLE        schService;

	if (NULL == lpszDriverName)
	{
		return FALSE;
	}

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		Dprint("fail open scmanager:%d", GetLastError());
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		Dprint("fail open service %s:%d", lpszDriverName, GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}

	if (!StartService(schService, 0, NULL))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
		{
			// 服务已经开启
			Dprint("service already exists");
			return TRUE;
		}
		Dprint("fail start service %s:%d", lpszDriverName, GetLastError());
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}

BOOL StopDriver(const char* lpszDriverName)
{
	SC_HANDLE        schManager;
	SC_HANDLE        schService;
	SERVICE_STATUS    svcStatus;
	bool            bStopped = false;

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		Dprint("fail open scmanager:%d", GetLastError());
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		Dprint("fail open service %s:%d", lpszDriverName, GetLastError());
		CloseServiceHandle(schManager);
		return FALSE;
	}
	if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
	{
		Dprint("fail control service %s:%d", lpszDriverName, GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}

BOOL DeleteDriver(const char* lpszDriverName)
{
	SC_HANDLE        schManager;
	SC_HANDLE        schService;
	SERVICE_STATUS    svcStatus;

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		Dprint("fail open scmanager:%d", GetLastError());
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		Dprint("fail open service %s:%d", lpszDriverName, GetLastError());
		CloseServiceHandle(schManager);
		return FALSE;
	}
	ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus);
	if (!DeleteService(schService))
	{
		Dprint("fail control service %s:%d", lpszDriverName, GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}

void usage(void) {
	Dprint("Usage:\n\tloader.exe install name path"
		"\n\tloader.exe start name"
		"\n\tloader.exe stop name"
		"\n\tloader.exe delete name"
		"\n q to quit\n"
	);
}

int do_operate(char *operation, char *name, char *path) {
	if (!strcmp("install", operation)) {
		if (InstallDriver(name, path, "370150")) {
			Dprint("success install driver:%s,%s", name, path);
			return TRUE;
		}
	}
	else if (!strcmp("start", operation)) {
		if (StartDriver(name)) {
			Dprint("succeess start driver:%s", name);
			return TRUE;
		}
	}
	else if (!strcmp("stop", operation)) {
		if (StopDriver(name)) {
			Dprint("succeess stop driver:%s", name);
			return TRUE;
		}
	}
	else if (!strcmp("delete", operation)) {
		if (DeleteDriver(name)) {
			Dprint("succeess delete driver:%s", name);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL FileExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void main(int argc, char *argv[]) {
	char operate[0x100], name[0x100];
	char path[0x100];
	if (argc == 1) {
		usage();
		while (1) {
			printf("\n>");
			scanf("%s", operate);
			if (!strcmp("q", operate)) {
				break;
			}
			scanf("%s", name);
			printf("%s,%s", operate, name);
			if (!strcmp("install", operate)) {
				scanf("%s", path);
				printf(",%s", path);
				if (!FileExists(path)) {
					printf("Wrong path!\n");
					continue;
				}
			}
			printf("\n");
			do_operate(operate, name, path);
		}
	}
	else {
		do_operate(argv[1], argv[2], argv[3]);
	}
}