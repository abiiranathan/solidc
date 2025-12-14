#ifndef SOLIDC_PLATFORM_H
#define SOLIDC_PLATFORM_H

/**
 * @file platform.h
 * @brief Cross-platform compatibility definitions
 */

// Windows-specific type definitions
#ifdef _WIN32
// ssize_t is not defined in Windows
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#endif

#else
// POSIX systems have ssize_t in sys/types.h
#include <sys/types.h>
#endif

#endif /* SOLIDC_PLATFORM_H */
