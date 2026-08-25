/**
 * @file poller_kqueue.c
 * @brief macOS/BSD kqueue backend for poller.h.
 *
 * kqueue maps cleanly onto the solidc poller API:
 *   - EV_CLEAR          <-> POLLER_EDGE
 *   - EV_EOF            <-> poller_event_is_hup()
 *   - EVFILT_READ/WRITE <-> POLLER_READ / POLLER_WRITE
 *   - udata             <-> registration node (for poller_event_fd/data)
 *
 * One kqueue quirk: a descriptor can carry EVFILT_READ and EVFILT_WRITE as
 * two independent filters. poller_mod() therefore disables the filter that
 * is not requested, mirroring epoll's single-mask semantics.
 */

#include "poller_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <unistd.h>

struct Poller {
    PollerBase base;
    int kqfd;
};

Poller* poller_new(void) {
    Poller* p = (Poller*)calloc(1, sizeof(Poller));
    if (!p) return NULL;

    if (!poller_regs_init(&p->base)) {
        free(p);
        return NULL;
    }

    p->kqfd = kqueue();
    if (p->kqfd < 0) {
        poller_regs_free(&p->base);
        free(p);
        return NULL;
    }
    return p;
}

void poller_free(Poller* p) {
    if (!p) return;
    close(p->kqfd);
    poller_regs_free(&p->base);
    free(p);
}

/** Appends one filter change to a kevent batch. Returns 0 on success. */
static int kev_add(struct kevent* out, int kqfd, int fd, int16_t filter, bool enable, void* reg) {
    if (enable) {
        EV_SET(out, fd, filter, EV_ADD | EV_CLEAR, 0, 0, reg);
    } else {
        /* Disable (not delete) so the sibling filter stays intact. */
        EV_SET(out, fd, filter, EV_DISABLE, 0, 0, reg);
    }
    return kevent(kqfd, out, 1, NULL, 0, NULL);
}

int poller_add(Poller* p, int fd, int events, void* data) {
    if (!p || fd < 0) {
        errno = EINVAL;
        return -1;
    }

    PollerReg* reg = poller_regs_put(&p->base, fd, data);
    if (!reg) {
        errno = ENOMEM;
        return -1;
    }

    struct kevent ev;
    bool want_read = (events & POLLER_READ) != 0;
    bool want_write = (events & POLLER_WRITE) != 0;

    if (kev_add(&ev, p->kqfd, fd, EVFILT_READ, want_read, reg) != 0) {
        poller_regs_remove(&p->base, fd);
        return -1;
    }
    if (kev_add(&ev, p->kqfd, fd, EVFILT_WRITE, want_write, reg) != 0) {
        poller_regs_remove(&p->base, fd);
        return -1;
    }
    return 0;
}

int poller_mod(Poller* p, int fd, int events, void* data) {
    if (!p || fd < 0) {
        errno = EINVAL;
        return -1;
    }

    PollerReg* reg = poller_regs_get(&p->base, fd);
    if (!reg) {
        errno = ENOENT;
        return -1;
    }
    reg->data = data;

    struct kevent ev;
    bool want_read = (events & POLLER_READ) != 0;
    bool want_write = (events & POLLER_WRITE) != 0;

    if (kev_add(&ev, p->kqfd, fd, EVFILT_READ, want_read, reg) != 0) return -1;
    return kev_add(&ev, p->kqfd, fd, EVFILT_WRITE, want_write, reg);
}

int poller_del(Poller* p, int fd) {
    if (!p || fd < 0) {
        errno = EINVAL;
        return -1;
    }

    struct kevent ev;
    /* EV_DELETE on a filter that was never added returns ENOENT; treat as
     * success so poller_del is forgiving like the epoll backend. */
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    if (kevent(p->kqfd, &ev, 1, NULL, 0, NULL) != 0 && errno != ENOENT) return -1;
    EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    if (kevent(p->kqfd, &ev, 1, NULL, 0, NULL) != 0 && errno != ENOENT) return -1;

    poller_regs_remove(&p->base, fd);
    return 0;
}

int poller_wait(Poller* p, PollerEvent* events, int max, int timeout_ms) {
    if (!p || !events || max <= 0) {
        errno = EINVAL;
        return -1;
    }

    struct kevent* kevs = (struct kevent*)malloc((size_t)max * sizeof(struct kevent));
    if (!kevs) {
        errno = ENOMEM;
        return -1;
    }

    struct timespec ts, *ts_ptr = NULL;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        ts_ptr = &ts;
    }

    int n = kevent(p->kqfd, NULL, 0, kevs, max, ts_ptr);
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            PollerReg* reg = (PollerReg*)kevs[i].udata;
            events[i].fd = reg ? reg->fd : -1;
            events[i].data = reg ? reg->data : NULL;
            events[i].readable = (kevs[i].filter == EVFILT_READ);
            events[i].writable = (kevs[i].filter == EVFILT_WRITE);
            events[i].error = (kevs[i].flags & EV_ERROR) != 0;
            events[i].hup = (kevs[i].flags & EV_EOF) != 0;
        }
    }

    free(kevs);
    return n;
}

int poller_event_fd(const PollerEvent* ev) { return ev ? ev->fd : -1; }

void* poller_event_data(const PollerEvent* ev) { return ev ? ev->data : NULL; }

bool poller_event_is_read(const PollerEvent* ev) { return ev && ev->readable; }

bool poller_event_is_write(const PollerEvent* ev) { return ev && ev->writable; }

bool poller_event_is_error(const PollerEvent* ev) { return ev && ev->error; }

bool poller_event_is_hup(const PollerEvent* ev) { return ev && ev->hup; }
