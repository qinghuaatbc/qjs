#include "process.hpp"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <time.h>

extern "C" {
#include "../quickjs/quickjs.h"
}

static JSValue js_process_exit(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    int code = 0;
    if (argc > 0) JS_ToInt32(ctx, &code, argv[0]);
    exit(code);
    return JS_UNDEFINED;
}

static JSValue js_process_cwd(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)))
        return JS_NewString(ctx, buf);
    return JS_ThrowInternalError(ctx, "getcwd failed");
}

static double _start_time = 0;

static JSValue js_process_uptime(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = ts.tv_sec + ts.tv_nsec / 1e9;
    return JS_NewFloat64(ctx, now - _start_time);
}

static JSValue js_process_hrtime(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt64(ctx, (int64_t)ts.tv_sec));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt64(ctx, (int64_t)ts.tv_nsec));
    return arr;
}

static JSValue js_stdout_write(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { fputs(s, stdout); JS_FreeCString(ctx, s); }
    }
    return JS_UNDEFINED;
}

static JSValue js_stderr_write(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { fputs(s, stderr); JS_FreeCString(ctx, s); }
    }
    return JS_UNDEFINED;
}

void js_init_process(JSContext *ctx, JSValue global, int argc, char **argv) {
    // Record start time for uptime()
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    _start_time = ts.tv_sec + ts.tv_nsec / 1e9;
    JSValue process = JS_NewObject(ctx);

    // process.argv
    JSValue js_argv = JS_NewArray(ctx);
    for (int i = 0; i < argc; i++) {
        JS_SetPropertyUint32(ctx, js_argv, (uint32_t)i, JS_NewString(ctx, argv[i]));
    }
    JS_SetPropertyStr(ctx, process, "argv", js_argv);

    // process.env
    extern char **environ;
    JSValue env = JS_NewObject(ctx);
    for (char **e = environ; *e; e++) {
        std::string entry(*e);
        size_t eq = entry.find('=');
        if (eq != std::string::npos) {
            JS_SetPropertyStr(ctx, env,
                entry.substr(0, eq).c_str(),
                JS_NewString(ctx, entry.substr(eq + 1).c_str()));
        }
    }
    JS_SetPropertyStr(ctx, process, "env", env);

    // process.version / platform
    JS_SetPropertyStr(ctx, process, "version",  JS_NewString(ctx, "v1.0.0"));
    JS_SetPropertyStr(ctx, process, "platform", JS_NewString(ctx, "darwin"));

    // process.exit
    JS_SetPropertyStr(ctx, process, "exit",
        JS_NewCFunction(ctx, js_process_exit, "exit", 1));
    JS_SetPropertyStr(ctx, process, "cwd",
        JS_NewCFunction(ctx, js_process_cwd, "cwd", 0));
    JS_SetPropertyStr(ctx, process, "uptime",
        JS_NewCFunction(ctx, js_process_uptime, "uptime", 0));
    JS_SetPropertyStr(ctx, process, "hrtime",
        JS_NewCFunction(ctx, js_process_hrtime, "hrtime", 0));

    // process.stdout / stderr
    JSValue stdout_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, stdout_obj, "write",
        JS_NewCFunction(ctx, js_stdout_write, "write", 1));
    JS_SetPropertyStr(ctx, process, "stdout", stdout_obj);

    JSValue stderr_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, stderr_obj, "write",
        JS_NewCFunction(ctx, js_stderr_write, "write", 1));
    JS_SetPropertyStr(ctx, process, "stderr", stderr_obj);

    JS_SetPropertyStr(ctx, global, "process", process);
}
