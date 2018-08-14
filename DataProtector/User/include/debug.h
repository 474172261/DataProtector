#pragma once

#define DEBUG_PRINT
#define DEBUG_LEVEL 0

#define LEVEL_INFO 0
#define LEVEL_INFO1 1
#define LEVEL_ERROR 2

#include <stdio.h>

int dprint(int level, const char *fmt, ...);

int dwprint(int level, const wchar_t *fmt, ...);