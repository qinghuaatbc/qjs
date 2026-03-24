#include "net.hpp"
#include <cstring>
#include <cerrno>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

extern "C" {
#include "../quickjs/quickjs.h"
}

// Set fd non-blocking
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ── __net_connect(port, host) → fd ────────────────────────────────────────────
// Does a blocking connect (with short timeout) then switches to non-blocking.
static JSValue js_net_connect(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "net_connect: expected (port, host)");

    int32_t port = 0;
    JS_ToInt32(ctx, &port, argv[0]);

    const char *host = "127.0.0.1";
    bool free_host = false;
    if (argc >= 2 && JS_IsString(argv[1])) {
        host = JS_ToCString(ctx, argv[1]);
        free_host = true;
    }

    // Resolve hostname
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    int r = getaddrinfo(host, portstr, &hints, &res);
    if (free_host) JS_FreeCString(ctx, host);
    if (r != 0 || !res) return JS_NewInt32(ctx, -1);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(res); return JS_NewInt32(ctx, -1); }

    int opt = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    // Blocking connect with select timeout (3 seconds)
    set_nonblocking(fd);
    int cr = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (cr < 0 && errno != EINPROGRESS) {
        close(fd); return JS_NewInt32(ctx, -1);
    }

    if (cr != 0) {
        // Wait up to 3s for connect
        fd_set wfds; FD_ZERO(&wfds); FD_SET(fd, &wfds);
        struct timeval tv{ 3, 0 };
        int sr = select(fd + 1, nullptr, &wfds, nullptr, &tv);
        if (sr <= 0) { close(fd); return JS_NewInt32(ctx, -1); }
        int err = 0;
        socklen_t elen = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
        if (err) { close(fd); return JS_NewInt32(ctx, -1); }
    }

    return JS_NewInt32(ctx, fd);
}

// ── __net_write(fd, data) → bool ─────────────────────────────────────────────
static JSValue js_net_write(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_FALSE;
    int32_t fd = 0;
    JS_ToInt32(ctx, &fd, argv[0]);
    size_t len = 0;
    const char *data = JS_ToCStringLen(ctx, &len, argv[1]);
    if (!data) return JS_FALSE;

    ssize_t total = 0;
    while (total < (ssize_t)len) {
        ssize_t n = write(fd, data + total, len - total);
        if (n < 0) { if (errno == EAGAIN || errno == EINTR) continue; break; }
        total += n;
    }
    JS_FreeCString(ctx, data);
    return JS_NewBool(ctx, total == (ssize_t)len);
}

// ── __net_read(fd, maxBytes) → string | null ──────────────────────────────────
static JSValue js_net_read(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NULL;
    int32_t fd = 0, maxbytes = 65536;
    JS_ToInt32(ctx, &fd, argv[0]);
    if (argc >= 2) JS_ToInt32(ctx, &maxbytes, argv[1]);

    std::string buf(maxbytes, '\0');
    ssize_t n = read(fd, &buf[0], maxbytes);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return JS_NULL;
        return JS_NULL;
    }
    if (n == 0) return JS_NULL;  // EOF
    return JS_NewStringLen(ctx, buf.c_str(), (size_t)n);
}

// ── __net_close(fd) ──────────────────────────────────────────────────────────
static JSValue js_net_close(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc >= 1) {
        int32_t fd = 0;
        JS_ToInt32(ctx, &fd, argv[0]);
        if (fd >= 0) close(fd);
    }
    return JS_UNDEFINED;
}

// ── install ───────────────────────────────────────────────────────────────────
void js_init_net(JSContext *ctx, JSValue global) {
    JS_SetPropertyStr(ctx, global, "__net_connect",
        JS_NewCFunction(ctx, js_net_connect, "__net_connect", 2));
    JS_SetPropertyStr(ctx, global, "__net_write",
        JS_NewCFunction(ctx, js_net_write,   "__net_write",   2));
    JS_SetPropertyStr(ctx, global, "__net_read",
        JS_NewCFunction(ctx, js_net_read,    "__net_read",    2));
    JS_SetPropertyStr(ctx, global, "__net_close",
        JS_NewCFunction(ctx, js_net_close,   "__net_close",   1));
}
