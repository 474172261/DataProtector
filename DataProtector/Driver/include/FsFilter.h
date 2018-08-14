#pragma once
#include <NTDDk.h>

#define FlagOnHigh(bits, flag) ((bits>>24) & (flag))

extern PFLT_FILTER ghFilter;

extern volatile LONG gEventCount;