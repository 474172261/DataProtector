#include <Windows.h>

wchar_t*
FromUTF8toWideChar(
	const char* src,
	int src_length,  /* = 0 */
	int* out_length  /* = NULL */
);

char*
toUTF8(
	const wchar_t* src,
	int src_length,  /* = 0 */
	int* out_length  /* = NULL */
);

int SilentlyRemoveDirectory(const WCHAR* dir);

int DeviceNameToVolumePathName(WCHAR *filepath);

int GetCurrentPath(WCHAR *path, int maxsize);

int IsProcessAlive(UINT32 pid);

int Stricmp(WCHAR *target, WCHAR *src);