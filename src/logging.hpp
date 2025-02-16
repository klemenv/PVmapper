#pragma once

#include <errlog.h>

#define LOG_DEBUG(args ...)    errlogSevPrintf(errlogInfo, args)
#define LOG_VERBOSE(args ...)  errlogSevPrintf(errlogMinor, args)
#define LOG_INFO(args ...)     errlogSevPrintf(errlogMajor, args)
#define LOG_ERROR(args ...)    errlogSevPrintf(errlogFatal, args)
