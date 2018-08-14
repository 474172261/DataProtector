#pragma once

#include <NTDDk.h>

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define TRACE_FLAG 0x1
#define POOL_TAG 'UxT1'
#define POOL_TAG_UNICODESTR 'UxT2'
#define POOL_TAG_IMGNAME 'UxT3'
#define POOL_TAG_MSG 'UxT4'
#define POOL_TAG_VLMNAME 'UxT5'
#define POOL_TAG_WHITELIST 'UxT6'
#define POOL_TAG_WHITELISTDIR 'UxT7'
#define POOL_TAG_UNICODEBUF 'UxT8'

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(TRACE_FLAG,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))