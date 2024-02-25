#ifndef FC8E8363_AF45_44FA_BD12_A6ED9E1E077D
#define FC8E8363_AF45_44FA_BD12_A6ED9E1E077D

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // Required for secure_getenv, strdup on Linux
#endif

#if defined SOLIDC_IMPL
#define SOLIDC_IMPL
#define OS_IMPL
#define STR_IMPL
#define TRIE_IMPL
#define VEC_IMPL
#define MAP_IMPL
#define ORDERED_MAP_IMPL
#define LIST_IMPL
#define SLIST_IMPL
#define LOG_IMPL
#define SOCKET_IMPL
#endif

#include "list.h"
#include "log.h"
#include "map.h"
#include "ordered_map.h"
#include "os.h"
#include "slist.h"
#include "socket.h"
#include "str.h"
#include "trie.h"
#if defined(__cplusplus)
}
#endif

#endif /* FC8E8363_AF45_44FA_BD12_A6ED9E1E077D */
