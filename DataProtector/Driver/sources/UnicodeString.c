#include <fltKernel.h>
#include "UnicodeString.h"
#include "debug.h"
#include "Helper.h"
#include "FsFilter.h"


WCHAR* gtcExtentionFiltTypes[] = { L"doc",L"docx",L"xls",L"xlsx",L"ppt",L"pptx",L"pst",L"ost",L"msg",
	L"eml",L"vsd",L"vsdx",L"txt",L"csv",L"rtf",L"123",L"wks",L"wk1",L"pdf",L"dwg",L"onetoc2",L"snt",
	L"jpeg",L"jpg",L"docb",L"docm",L"dot",L"dotm",L"dotx",L"xlsm",L"xlsb",L"xlw",L"xlt",L"xlm",L"xlc",
	L"xltx",L"xltm",L"pptm",L"pot",L"pps",L"ppsm",L"ppsx",L"ppam",L"potx",L"potm",L"edb",L"hwp",
	L"602",L"sxi",L"sti",L"sldx",L"sldm",L"sldm",L"vdi",L"vmdk",L"vmx",L"gpg",L"aes",L"ARC",L"PAQ",
	L"bz2",L"tbk",L"bak",L"tar",L"tgz",L"gz",L"7z",L"rar",L"zip",L"backup",L"iso",L"vcd",L"bmp",
	L"png",L"gif",L"raw",L"cgm",L"tif",L"tiff",L"nef",L"psd",L"ai",L"svg",L"djvu",L"m4u",L"m3u",
	L"mid",L"wma",L"flv",L"3g2",L"mkv",L"3gp",L"mp4",L"mov",L"avi",L"asf",L"mpeg",L"vob",L"mpg",
	L"wmv",L"fla",L"swf",L"wav",L"mp3",L"sh",L"class",L"jar",L"java",L"rb",L"asp",L"php",L"jsp",
	L"brd",L"sch",L"dch",L"dip",L"pl",L"vb",L"vbs",L"ps1",L"bat",L"cmd",L"js",L"asm",L"h",L"pas",
	L"cpp",L"c",L"cs",L"suo",L"sln",L"ldf",L"mdf",L"ibd",L"myi",L"myd",L"frm",L"odb",L"dbf",L"db",
	L"mdb",L"accdb",L"sql",L"sqlitedb",L"sqlite3",L"asc",L"lay6",L"lay",L"mml",L"sxm",L"otg",L"odg",
	L"uop",L"std",L"sxd",L"otp",L"odp",L"wb2",L"slk",L"dif",L"stc",L"sxc",L"ots",L"ods",L"3dm",L"max",
	L"3ds",L"uot",L"stw",L"sxw",L"ott",L"odt",L"pem",L"p12",L"csr",L"crt",L"key",L"pfx",L"der" };

UNICODE_STRING_ARRAY guExtFltTypes = { 0 };
UNICODE_STRING_POINTER_ARRAY guWhiteListDir = { 0 };
UNICODE_STRING_POINTER_ARRAY guWhiteList = { 0 };

NTSTATUS InitFilterUnicodeString(VOID) {
	UINT32 typeSize;
	UNICODE_STRING *pUnicodeStr;
	UINT32 initUnicodeCount;

	PAGED_CODE();

	// prepare UnicodeString
	typeSize = sizeof(gtcExtentionFiltTypes) / sizeof(WCHAR *);

	// alloc heap
	pUnicodeStr = ExAllocatePoolWithTag(PagedPool,
		typeSize * sizeof(UNICODE_STRING), 
		POOL_TAG_UNICODESTR);

	if (!pUnicodeStr){
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!InitFilterUnicodeString: Can't alloc heap for unicode string.size:%x.\n",
				typeSize));
		return STATUS_UNSUCCESSFUL;
	}
	// init variable
	guExtFltTypes.count = typeSize;
	guExtFltTypes.pUnicodeStr = pUnicodeStr;
	// init unicode string
	initUnicodeCount = 0;
	for (UINT32 i = 0; i < typeSize; i++) {
		if (!RtlCreateUnicodeString(&pUnicodeStr[i], gtcExtentionFiltTypes[i])) {
			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
				("DPfilter!InitFilterUnicodeString: Can't init all unicode string.\n"));
			break;
		}
		initUnicodeCount++;
	}
	// check if init all string,if fail, free them all and return fail.
	if (initUnicodeCount != typeSize) {
		for (UINT32 i = 0; i < initUnicodeCount; i++) {
			RtlFreeUnicodeString(&pUnicodeStr[i]);
		}
		guExtFltTypes.count = 0;
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

VOID UnInitFilterUnicodeString(VOID) {
	UNICODE_STRING *pUnicodeStr = guExtFltTypes.pUnicodeStr;

	PAGED_CODE();

	if (pUnicodeStr) {
		for (int i = 0; i < guExtFltTypes.count; i++) {
			RtlFreeUnicodeString(&pUnicodeStr[i]);
		}
		ExFreePoolWithTag(pUnicodeStr, POOL_TAG_UNICODESTR);
		guExtFltTypes.pUnicodeStr = NULL;
	}
}

NTSTATUS InitBasicUnicodeString(void) {
	PUNICODE_STRING *ppUnicode;
	PUNICODE_STRING puSystemRoot = NULL;
	UNICODE_STRING uWindows;
	UNICODE_STRING uExplorer;
	NTSTATUS status;

	// Init WhiteListDir
	ppUnicode = ExAllocatePoolWithTag(NonPagedPool,
		WHITELISTDIR_MAX * sizeof(PUNICODE_STRING),
		POOL_TAG_WHITELISTDIR);

	if (!ppUnicode) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!InitBasicUnicodeString: Can't init guWhiteListDir.\n"));
		status = STATUS_UNSUCCESSFUL;
		goto L_Clean;
	}
	guWhiteListDir.ppUnicodeStrArray = ppUnicode;
	if (!NT_SUCCESS(GetVolumeNameByName(L"\\SystemRoot", &puSystemRoot))) {
		status = STATUS_UNSUCCESSFUL;
		goto L_Clean;
	}
	
	RtlInitUnicodeString(&uWindows, L"\\Windows\\");
	ppUnicode[0] = ConcatUnicodeString(puSystemRoot, &uWindows);
	if (!ppUnicode[0]) {
		status = STATUS_UNSUCCESSFUL;
		goto L_Clean;
	}
	guWhiteListDir.count = 1;

	// Init WhiteList
	ppUnicode = ExAllocatePoolWithTag(NonPagedPool,
		WHITELIST_MAX * sizeof(PUNICODE_STRING),
		POOL_TAG_WHITELIST);

	if (!ppUnicode) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("DPfilter!InitBasicUnicodeString: Can't init guWhiteList.\n"));
		status = STATUS_UNSUCCESSFUL;
		goto L_Clean;
	}
	guWhiteList.ppUnicodeStrArray = ppUnicode;
	RtlInitUnicodeString(&uExplorer, L"\\Windows\\explorer.exe");
	ppUnicode[0] = ConcatUnicodeString(puSystemRoot, &uExplorer);
	if (!ppUnicode[0]) {
		status = STATUS_UNSUCCESSFUL;
		goto L_Clean;
	}
	guWhiteList.count = 1;
	
L_Clean:
	MyFreeUnicodeString(puSystemRoot);

	return STATUS_SUCCESS;
}

void UnInitBasicUnicodeString(void) {
	PUNICODE_STRING *ppUnicode;
	
	ppUnicode = guWhiteListDir.ppUnicodeStrArray;
	if (ppUnicode) {
		for (int i = 0; i < guWhiteListDir.count; i++) {
			MyFreeUnicodeString(ppUnicode[i]);
		}
		ExFreePoolWithTag(ppUnicode, POOL_TAG_WHITELISTDIR);
	}

	ppUnicode = guWhiteList.ppUnicodeStrArray;
	if (ppUnicode) {
		for (int i = 0; i < guWhiteList.count; i++) {
			MyFreeUnicodeString(ppUnicode[i]);
		}
		ExFreePoolWithTag(ppUnicode, POOL_TAG_WHITELIST);
	}
}