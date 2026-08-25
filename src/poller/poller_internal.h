/**
 * @file poller_internal.h
 * @brief Shared state between the poller backends (epoll / kqueue / win32).
 *
 * The public API promises poller_event_fd() always works. epoll and kqueue
 * deliver only the opaque user pointer, so each backend keeps an internal
 * fd -> registration map and stores a pointer to the registration node in
 * the platform's user-data field. WSAPoll reports the fd directly but uses
 * the map anyway for uniformity and for mod/del bookkeeping.
 */

#ifndef SOLIDC_POLLER_INTERNAL_H
#define SOLIDC_POLLER_INTERNAL_H

#include "../../include/poller.h"

#include <stdlib.h>
#include <string.h>

/** Per-descriptor registration node. Never moves once allocated. */
typedef struct PollerReg {
    int fd;
    void* data;
    int events; /**< Interest mask (POLLER_READ/WRITE); used by win32. */
    bool active;
} PollerReg;

/**
 * Open-addressing hash table keyed by fd. Power-of-two capacity, grows at
 * 50% load. Lookup is O(1); pollers handle thousands of fds, so this keeps
 * per-event dispatch constant-time.
 */
typedef struct PollerRegTable {
    PollerReg* slots;
    size_t capacity; /* power of two */
    size_t count;
} PollerRegTable;

/** Shared poller base embedded in every backend's Poller struct. */
typedef struct PollerBase {
    PollerRegTable regs;
} PollerBase;

/** Initializes the registration table. @return true on success. */
bool poller_regs_init(PollerBase* base);

/** Frees the registration table (not the descriptors). */
void poller_regs_free(PollerBase* base);

/** Inserts or updates the registration for @p fd. @return node, NULL on OOM. */
PollerReg* poller_regs_put(PollerBase* base, int fd, void* data);

/** Finds the registration for @p fd, or NULL. */
PollerReg* poller_regs_get(PollerBase* base, int fd);

/** Removes (deactivates) the registration for @p fd. @return true if found. */
bool poller_regs_remove(PollerBase* base, int fd);

#endif /* SOLIDC_POLLER_INTERNAL_H */
