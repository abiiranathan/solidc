/**
 * @file poller_win32.c
 * @brief Windows WSAPoll backend for poller.h.
 *
 * Semantics note
 * --------------
 * WSAPoll is LEVEL-triggered: POLLER_EDGE is accepted and ignored, and
 * callers must be written for level semantics (or use IOCP directly).
 * WSAPoll also only works on sockets, which matches solidc's socket.h.
 *
 * WSAPoll reports the fd directly (POLLFD.fd), so the registration map is
 * used for mod/del bookkeeping and to keep poller_event_fd()/data()
 * uniform across backends.
 */

#include "poller_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/platform.h"

struct Poller {
    PollerBase base;
    WSAPOLLFD* fds; /* parallel to regs order we maintain manually */
    void** datas;
    size_t fd_count;
    size_t fd_capacity;
};

Poller* poller_new(void) {
    Poller* p = (Poller*)calloc(1, sizeof(Poller));
    if (!p) return NULL;

    if (!poller_regs_init(&p->base)) {
        free(p);
        return NULL;
    }
    return p;
}

void poller_free(Poller* p) {
    if (!p) return;
    free(p->fds);
    free(p->datas);
    poller_regs_free(&p->base);
    free(p);
}

/** Rebuilds the WSAPOLLFD array from the registration table. Called with no
 * lock (the poller is single-threaded by contract). @return 0 on success. */
static int poller_rebuild(Poller* p) {
    if (p->fd_capacity < p->base.regs.count) {
        size_t new_cap = p->base.regs.count < 16 ? 16 : p->base.regs.count * 2;
        WSAPOLLFD* nf = (WSAPOLLFD*)realloc(p->fds, new_cap * sizeof(WSAPOLLFD));
        if (!nf) {
            errno = ENOMEM;
            return -1;
        }
        p->fds = nf;
        void** nd = (void**)realloc(p->datas, new_cap * sizeof(void*));
        if (!nd) {
            errno = ENOMEM;
            return -1;
        }
        p->datas = nd;
        p->fd_capacity = new_cap;
    }

    size_t n = 0;
    for (size_t i = 0; i < p->base.regs.capacity; i++) {
        PollerReg* r = &p->base.regs.slots[i];
        if (!r->active) continue;
        p->fds[n].fd = r->fd;
        p->fds[n].events = (short)((r->events & POLLER_READ) ? (POLLRDNORM | POLLRDBAND) : 0);
        if (r->events & POLLER_WRITE) p->fds[n].events |= POLLWRNORM;
        p->fds[n].revents = 0;
        p->datas[n] = r->data;
        n++;
    }
    p->fd_count = n;
    return 0;
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
    reg->events = events & (POLLER_READ | POLLER_WRITE);
    return poller_rebuild(p);
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
    reg->events = events & (POLLER_READ | POLLER_WRITE);
    return poller_rebuild(p);
}

int poller_del(Poller* p, int fd) {
    if (!p || fd < 0) {
        errno = EINVAL;
        return -1;
    }
    poller_regs_remove(&p->base, fd);
    return poller_rebuild(p);
}

int poller_wait(Poller* p, PollerEvent* events, int max, int timeout_ms) {
    if (!p || !events || max <= 0) {
        errno = EINVAL;
        return -1;
    }
    if (p->fd_count == 0) {
        if (timeout_ms > 0) Sleep((DWORD)timeout_ms);
        return 0;
    }

    int n = WSAPoll(p->fds, (ULONG)p->fd_count, timeout_ms);
    if (n <= 0) return n;

    int out = 0;
    for (size_t i = 0; i < p->fd_count && out < max; i++) {
        SHORT re = p->fds[i].revents;
        if (!re) continue;

        events[out].fd = (int)p->fds[i].fd;
        events[out].data = p->datas[i];
        events[out].readable = (re & (POLLRDNORM | POLLRDBAND)) != 0;
        events[out].writable = (re & POLLWRNORM) != 0;
        events[out].error = (re & POLLERR) != 0;
        events[out].hup = (re & POLLHUP) != 0; /* WSAPoll has no RDHUP */
        out++;
    }
    return out;
}

int poller_event_fd(const PollerEvent* ev) { return ev ? ev->fd : -1; }

void* poller_event_data(const PollerEvent* ev) { return ev ? ev->data : NULL; }

bool poller_event_is_read(const PollerEvent* ev) { return ev && ev->readable; }

bool poller_event_is_write(const PollerEvent* ev) { return ev && ev->writable; }

bool poller_event_is_error(const PollerEvent* ev) { return ev && ev->error; }

bool poller_event_is_hup(const PollerEvent* ev) { return ev && ev->hup; }
