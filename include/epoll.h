#ifndef SOLIDC_EPOLL_H
#define SOLIDC_EPOLL_H

/**
 * @file epoll.h
 * @brief Cross-platform event notification API (epoll/kqueue abstraction)
 *
 * Provides a unified interface for Linux epoll and BSD/macOS kqueue.
 * This is a standalone API imported on-demand for POSIX systems.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__linux__)
#include <sys/epoll.h>  // for epoll_create1, epoll_ctl, epoll_wait
#elif defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/event.h>  // for kqueue, kevent, EVFILT_READ, EVFILT_WRITE
#endif

/* ================================================================
 * Platform Detection
 * ================================================================ */

#if defined(__linux__)
#define SOLIDC_USE_EPOLL  1
#define SOLIDC_USE_KQUEUE 0
#elif defined(__APPLE__) || defined(__FreeBSD__)
#define SOLIDC_USE_EPOLL  0
#define SOLIDC_USE_KQUEUE 1
#else
#error "Unsupported platform: only Linux, macOS, and FreeBSD are supported"
#endif

/* ================================================================
 * Event Flags (epoll-style API)
 * ================================================================ */

#if SOLIDC_USE_EPOLL
/** Read event flag */
#define SOLIDC_EPOLLIN EPOLLIN
/** Write event flag */
#define SOLIDC_EPOLLOUT EPOLLOUT
/** Error condition */
#define SOLIDC_EPOLLERR EPOLLERR
/** Hang up */
#define SOLIDC_EPOLLHUP EPOLLHUP
/** Peer closed connection */
#define SOLIDC_EPOLLRDHUP EPOLLRDHUP
/** Edge-triggered mode */
#define SOLIDC_EPOLLET EPOLLET
/** Exclusive wakeup (Linux 4.5+) */
#define SOLIDC_EPOLLEXCLUSIVE EPOLLEXCLUSIVE

#elif SOLIDC_USE_KQUEUE
/** Read event flag */
#define SOLIDC_EPOLLIN        (1U << 0)
/** Write event flag */
#define SOLIDC_EPOLLOUT       (1U << 1)
/** Error condition */
#define SOLIDC_EPOLLERR       (1U << 2)
/** Hang up */
#define SOLIDC_EPOLLHUP       (1U << 3)
/** Peer closed connection */
#define SOLIDC_EPOLLRDHUP     (1U << 4)
/** Edge-triggered mode */
#define SOLIDC_EPOLLET        (1U << 5)
/** Exclusive wakeup (not applicable for kqueue) */
#define SOLIDC_EPOLLEXCLUSIVE 0
#endif

/* ================================================================
 * Event Data Union
 * ================================================================ */

/**
 * Event data union - allows storing either a pointer or file descriptor.
 * Similar to epoll_data_t but with clearer naming.
 */
typedef union {
    void* ptr;    /** User-defined pointer */
    int fd;       /** File descriptor */
    uint32_t u32; /** 32-bit unsigned integer */
    uint64_t u64; /** 64-bit unsigned integer */
} EpollData;

/**
 * Event structure compatible with both epoll and kqueue.
 */
typedef struct {
    uint32_t events; /** Event flags (SOLIDC_EPOLLIN, etc.) */
    EpollData data;  /** User data */
} EpollEvent;

/* ================================================================
 * Core API Functions
 * ================================================================ */

/**
 * Creates a new epoll/kqueue instance.
 * @return Event queue file descriptor on success, -1 on error.
 */
static inline int EpollCreate(void) {
#if SOLIDC_USE_EPOLL
    return epoll_create1(0);
#elif SOLIDC_USE_KQUEUE
    return kqueue();
#endif
}

/**
 * Adds a file descriptor to the event queue.
 * @param epfd Event queue descriptor.
 * @param fd File descriptor to monitor.
 * @param event Event structure with flags and user data.
 * @return 0 on success, -1 on error.
 */
static inline int EpollCtrlAdd(int epfd, int fd, EpollEvent* event) {
#if SOLIDC_USE_EPOLL
    struct epoll_event ev;
    ev.events = event->events;
    ev.data   = *((epoll_data_t*)&event->data);
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
#elif SOLIDC_USE_KQUEUE
    struct kevent kev[2];
    int nchanges = 0;

    if (event->events & SOLIDC_EPOLLIN) {
        uint16_t flags = EV_ADD;
        if (event->events & SOLIDC_EPOLLET) {
            flags |= EV_CLEAR;
        }
        EV_SET(&kev[nchanges++], fd, EVFILT_READ, flags, 0, 0, event->data.ptr);
    }

    if (event->events & SOLIDC_EPOLLOUT) {
        uint16_t flags = EV_ADD;
        if (event->events & SOLIDC_EPOLLET) {
            flags |= EV_CLEAR;
        }
        EV_SET(&kev[nchanges++], fd, EVFILT_WRITE, flags, 0, 0, event->data.ptr);
    }

    return (nchanges > 0) ? kevent(epfd, kev, nchanges, NULL, 0, NULL) : -1;
#endif
}

/**
 * Modifies the events associated with a file descriptor.
 * @param epfd Event queue descriptor.
 * @param fd File descriptor to modify.
 * @param event New event structure with updated flags and user data.
 * @return 0 on success, -1 on error.
 */
static inline int EpollCtrlMod(int epfd, int fd, EpollEvent* event) {
#if SOLIDC_USE_EPOLL
    struct epoll_event ev;
    ev.events = event->events;
    ev.data   = *((epoll_data_t*)&event->data);
    return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
#elif SOLIDC_USE_KQUEUE
    // kqueue doesn't have a "modify" operation, so we delete and re-add
    struct kevent kev[4];
    int nchanges = 0;

    // Delete existing filters
    EV_SET(&kev[nchanges++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&kev[nchanges++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    // Add new filters based on requested events
    if (event->events & SOLIDC_EPOLLIN) {
        uint16_t flags = EV_ADD;
        if (event->events & SOLIDC_EPOLLET) {
            flags |= EV_CLEAR;
        }
        EV_SET(&kev[nchanges++], fd, EVFILT_READ, flags, 0, 0, event->data.ptr);
    }

    if (event->events & SOLIDC_EPOLLOUT) {
        uint16_t flags = EV_ADD;
        if (event->events & SOLIDC_EPOLLET) {
            flags |= EV_CLEAR;
        }
        EV_SET(&kev[nchanges++], fd, EVFILT_WRITE, flags, 0, 0, event->data.ptr);
    }

    return kevent(epfd, kev, nchanges, NULL, 0, NULL);
#endif
}

/**
 * Removes a file descriptor from the event queue.
 * @param epfd Event queue descriptor.
 * @param fd File descriptor to remove.
 * @return 0 on success, -1 on error.
 */
static inline int EpollCtrlDel(int epfd, int fd) {
#if SOLIDC_USE_EPOLL
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
#elif SOLIDC_USE_KQUEUE
    struct kevent kev[2];
    // Try to delete both read and write filters
    EV_SET(&kev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&kev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    // kqueue auto-removes on fd close, so don't fail if delete fails
    kevent(epfd, kev, 2, NULL, 0, NULL);
    return 0;
#endif
}

/**
 * Waits for events on the event queue.
 * @param epfd Event queue descriptor.
 * @param events Array to store returned events (allocated by caller).
 * @param maxevents Maximum number of events to return (size of events array).
 * @param timeout_ms Timeout in milliseconds (-1 for infinite, 0 for immediate).
 * @return Number of ready events on success, -1 on error.
 */
static inline int EpollWait(int epfd, EpollEvent* events, size_t maxevents, int timeout_ms) {
#if SOLIDC_USE_EPOLL
    // On Linux, solidc_epoll_event_t layout matches epoll_event exactly,
    // so we can cast directly without copying
    return epoll_wait(epfd, (struct epoll_event*)events, maxevents, timeout_ms);
#elif SOLIDC_USE_KQUEUE
    struct timespec timeout;
    struct timespec* timeout_ptr = NULL;

    if (timeout_ms >= 0) {
        timeout.tv_sec  = timeout_ms / 1000;
        timeout.tv_nsec = (timeout_ms % 1000) * 1000000L;
        timeout_ptr     = &timeout;
    }

    // Reuse the events array by casting to struct kevent temporarily
    // This works because we'll overwrite it immediately after
    struct kevent* kev_array = (struct kevent*)events;
    int nfds                 = kevent(epfd, NULL, 0, kev_array, maxevents, timeout_ptr);

    // Convert kevent results back to solidc_epoll_event_t in-place
    // Process in reverse to avoid overwriting data we haven't read yet
    for (int i = nfds - 1; i >= 0; i--) {
        struct kevent* kev = &kev_array[i];

        uint32_t evt = 0;
        void* udata  = kev->udata;

        if (kev->filter == EVFILT_READ) {
            evt |= SOLIDC_EPOLLIN;
        } else if (kev->filter == EVFILT_WRITE) {
            evt |= SOLIDC_EPOLLOUT;
        }

        if (kev->flags & EV_EOF) {
            evt |= SOLIDC_EPOLLHUP;
            if (kev->filter == EVFILT_READ) {
                evt |= SOLIDC_EPOLLRDHUP;
            }
        }

        if (kev->flags & EV_ERROR) {
            evt |= SOLIDC_EPOLLERR;
        }

        // Now write to the events array
        events[i].events   = evt;
        events[i].data.ptr = udata;
    }

    return nfds;
#endif
}

#endif /* SOLIDC_EPOLL_H */
