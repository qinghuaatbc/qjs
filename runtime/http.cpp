#include "http.hpp"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <string>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

extern "C" {
#include "../quickjs/quickjs.h"
}

// ── low-level socket helpers ──────────────────────────────────────────────────

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ── JS-callable socket primitives ─────────────────────────────────────────────

// __http_create_server_socket(port, host?) → fd (number) | throws
static JSValue js_http_create_server_socket(JSContext *ctx, JSValueConst,
                                             int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "createServerSocket: port required");

    int32_t port;
    JS_ToInt32(ctx, &port, argv[0]);

    const char *host = "0.0.0.0";
    const char *host_str = nullptr;
    if (argc > 1 && JS_IsString(argv[1])) {
        host_str = JS_ToCString(ctx, argv[1]);
        if (host_str) host = host_str;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (host_str) JS_FreeCString(ctx, host_str);
        return JS_ThrowInternalError(ctx, "socket() failed: %s", strerror(errno));
    }

    // Allow immediate reuse after restart
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    // Disable Nagle for lower latency
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (host_str) JS_FreeCString(ctx, host_str);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return JS_ThrowInternalError(ctx, "bind() port %d: %s", port, strerror(errno));
    }

    if (listen(fd, 128) < 0) {
        close(fd);
        return JS_ThrowInternalError(ctx, "listen() failed: %s", strerror(errno));
    }

    set_nonblocking(fd);
    return JS_NewInt32(ctx, fd);
}

// __http_accept(serverFd) → clientFd (number) | -1
static JSValue js_http_accept(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewInt32(ctx, -1);

    int32_t server_fd;
    JS_ToInt32(ctx, &server_fd, argv[0]);

    struct sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return JS_NewInt32(ctx, -1);
        return JS_ThrowInternalError(ctx, "accept() failed: %s", strerror(errno));
    }

    set_nonblocking(client_fd);

    // Return { fd, remoteAddr }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "fd",         JS_NewInt32(ctx, client_fd));
    JS_SetPropertyStr(ctx, obj, "remoteAddr", JS_NewString(ctx, ip));
    JS_SetPropertyStr(ctx, obj, "remotePort",
        JS_NewInt32(ctx, ntohs(client_addr.sin_port)));
    return obj;
}

// __http_read(fd, maxBytes?) → string | null
static JSValue js_http_read(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NULL;

    int32_t fd;
    JS_ToInt32(ctx, &fd, argv[0]);

    int32_t max_bytes = 65536;
    if (argc > 1) JS_ToInt32(ctx, &max_bytes, argv[1]);

    std::string buf(max_bytes, '\0');
    ssize_t n = read(fd, &buf[0], max_bytes);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return JS_NULL;
        return JS_NULL;  // treat other errors as EOF
    }
    if (n == 0) return JS_NULL;  // EOF / connection closed

    return JS_NewStringLen(ctx, buf.c_str(), (size_t)n);
}

// __http_write(fd, data) → bytesWritten
static JSValue js_http_write(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_NewInt32(ctx, 0);

    int32_t fd;
    JS_ToInt32(ctx, &fd, argv[0]);

    size_t len;
    const char *data = JS_ToCStringLen(ctx, &len, argv[1]);
    if (!data) return JS_NewInt32(ctx, 0);

    ssize_t total = 0;
    while (total < (ssize_t)len) {
        ssize_t n = write(fd, data + total, len - total);
        if (n < 0) {
            if (errno == EAGAIN) continue;
            break;
        }
        total += n;
    }

    JS_FreeCString(ctx, data);
    return JS_NewInt32(ctx, (int32_t)total);
}

// __http_close(fd)
static JSValue js_http_close(JSContext *, JSValueConst, int argc, JSValueConst *argv) {
    if (argc > 0) {
        int32_t fd;
        JS_ToInt32(nullptr, &fd, argv[0]);
        // manually decode the int
        if (JS_VALUE_GET_TAG(argv[0]) == JS_TAG_INT)
            fd = JS_VALUE_GET_INT(argv[0]);
        else
            return JS_UNDEFINED;
        close(fd);
    }
    return JS_UNDEFINED;
}

// ── install ───────────────────────────────────────────────────────────────────

void js_init_http(JSContext *ctx, JSValue global) {
    JS_SetPropertyStr(ctx, global, "__http_create_server_socket",
        JS_NewCFunction(ctx, js_http_create_server_socket, "__http_create_server_socket", 2));
    JS_SetPropertyStr(ctx, global, "__http_accept",
        JS_NewCFunction(ctx, js_http_accept, "__http_accept", 1));
    JS_SetPropertyStr(ctx, global, "__http_read",
        JS_NewCFunction(ctx, js_http_read, "__http_read", 2));
    JS_SetPropertyStr(ctx, global, "__http_write",
        JS_NewCFunction(ctx, js_http_write, "__http_write", 2));
    JS_SetPropertyStr(ctx, global, "__http_close",
        JS_NewCFunction(ctx, js_http_close, "__http_close", 1));
}
