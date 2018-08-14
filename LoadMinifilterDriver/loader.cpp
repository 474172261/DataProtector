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

//======================================== ��̬����/ж��sys���� ======================================
// SYS�ļ����������ͬ��Ŀ¼��
// ���������SYS��ΪHelloDDK.sys,��ô��װ����InstallDriver("HelloDDK",".\\HelloDDK.sys","370030"/*Altitude*/);
// ������������ StartDriver("HelloDDK");
// ֹͣ�������� StopDriver("HelloDDK");
// ж��SYSҲ�����Ƶĵ��ù��̣� DeleteDriver("HelloDDK");
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
	//�õ�����������·��
	GetFullPathName(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);

	SC_HANDLE hServiceMgr = NULL;// SCM�������ľ��
	SC_HANDLE hService = NULL;// NT��������ķ�����

							  //�򿪷�����ƹ�����
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		// OpenSCManagerʧ��
		Dprint("fail open service:%d", GetLastError());
		CloseServiceHandle(hServiceMgr);
		return FALSE;
	}

	// OpenSCManager�ɹ�  

	//������������Ӧ�ķ���
	hService = CreateService(hServiceMgr,
		lpszDriverName,             // �����������ע����е�����
		lpszDriverName,             // ע������������DisplayName ֵ
		SERVICE_ALL_ACCESS,         // ������������ķ���Ȩ��
		SERVICE_FILE_SYSTEM_DRIVER, // ��ʾ���صķ������ļ�ϵͳ��������
		SERVICE_DEMAND_START,       // ע������������Start ֵ
		SERVICE_ERROR_IGNORE,       // ע������������ErrorControl ֵ
		szDriverImagePath,          // ע������������ImagePath ֵ
		"FSFilter Activity Monitor",// ע������������Group ֵ
		NULL,
		"FltMgr",                   // ע������������DependOnService ֵ
		NULL,
		NULL);

	if (hService == NULL)
	{
		if (GetLastError() == ERROR_SERVICE_EXISTS)
		{
			//���񴴽�ʧ�ܣ������ڷ����Ѿ�������
			CloseServiceHandle(hService);       // ������
			CloseServiceHandle(hServiceMgr);    // SCM���
			Dprint("already exists.");
			return TRUE;
		}
		else
		{
			CloseServiceHandle(hService);       // ������
			CloseServiceHandle(hServiceMgr);    // SCM���
			Dprint("fail create service:%d", GetLastError());
			return FALSE;
		}
	}
	CloseServiceHandle(hService);       // ������
	CloseServiceHandle(hServiceMgr);    // SCM���

										//-------------------------------------------------------------------------------------------------------
										// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances�ӽ��µļ�ֵ�� 
										//-------------------------------------------------------------------------------------------------------
	strcpy(szTempStr, "SYSTEM\\CurrentControlSet\\Services\\");
	strcat(szTempStr, lpszDriverName);
	strcat(szTempStr, "\\Instances");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, "", TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		Dprint("fail create key Instances:%d", GetLastError());
		return FALSE;
	}
	// ע������������DefaultInstance ֵ 
	strcpy(szTempStr, lpszDriverName);
	strcat(szTempStr, " Instance");
	if (RegSetValueEx(hKey, "DefaultInstance", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)strlen(szTempStr)) != ERROR_SUCCESS)
	{
		Dprint("fail set DefaultInstance:%d", GetLastError());
		return FALSE;
	}
	RegFlushKey(hKey);//ˢ��ע���
	RegCloseKey(hKey);
	//-------------------------------------------------------------------------------------------------------

	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance�ӽ��µļ�ֵ�� 
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
	// ע������������Altitude ֵ
	strcpy(szTempStr, lpszAltitude);
	if (RegSetValueEx(hKey, "Altitude", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)strlen(szTempStr)) != ERROR_SUCCESS)
	{
		Dprint("fail create value altitude:%d", GetLastError());
		return FALSE;
	}
	// ע������������Flags ֵ
	dwData = 0x0;
	if (RegSetValueEx(hKey, "Flags", 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		Dprint("fail create value flags:%d", GetLastError());
		return FALSE;
	}
	RegFlushKey(hKey);//ˢ��ע���
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
			// �����Ѿ�����
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