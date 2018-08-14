#pragma once
#include <NTDDk.h>

#define WHITELIST_MAX 1
#define WHITELISTDIR_MAX 1

typedef struct {
	int count;
	UNICODE_STRING *pUnicodeStr;
} UNICODE_STRING_ARRAY;

typedef struct {
	int count;
	UNICODE_STRING **ppUnicodeStrArray;
} UNICODE_STRING_POINTER_ARRAY;

extern UNICODE_STRING_ARRAY guExtFltTypes;
extern UNICODE_STRING_POINTER_ARRAY guWhiteList;
extern UNICODE_STRING_POINTER_ARRAY guWhiteListDir;


NTSTATUS InitFilterUnicodeString(VOID);
VOID UnInitFilterUnicodeString(VOID);

NTSTATUS InitBasicUnicodeString(void);
void UnInitBasicUnicodeString(void);
