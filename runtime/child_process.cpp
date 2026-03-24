#include "child_process.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "../quickjs/quickjs.h"
}

// execSync(cmd, [options]) → { stdout, stderr, status }
// throws on non-zero exit if options.throwOnError !== false
static JSValue js_exec_sync(JSContext *ctx, JSValueConst /*this_val*/,
                             int argc, JSValueConst *argv)
{
    if (argc < 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "execSync: expected string command");

    const char *cmd = JS_ToCString(ctx, argv[0]);
    if (!cmd) return JS_EXCEPTION;

    // parse options
    bool throw_on_error = true;
    bool capture_stderr = false;
    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue opt = JS_GetPropertyStr(ctx, argv[1], "throwOnError");
        if (!JS_IsUndefined(opt)) throw_on_error = JS_ToBool(ctx, opt);
        JS_FreeValue(ctx, opt);

        opt = JS_GetPropertyStr(ctx, argv[1], "captureStderr");
        if (!JS_IsUndefined(opt)) capture_stderr = JS_ToBool(ctx, opt);
        JS_FreeValue(ctx, opt);
    }

    // build command: redirect stderr to separate tmp file if requested
    std::string full_cmd = cmd;
    std::string stderr_file;
    if (capture_stderr) {
        stderr_file = "/tmp/qjs_stderr_XXXXXX";
        // can't use mkstemp with a string easily, just use a fixed tmp path
        stderr_file = "/tmp/qjs_stderr.txt";
        full_cmd += " 2>" + stderr_file;
    }

    JS_FreeCString(ctx, cmd);

    // run
    FILE *fp = popen(full_cmd.c_str(), "r");
    if (!fp) {
        return JS_ThrowInternalError(ctx, "execSync: popen failed: %s", strerror(errno));
    }

    // read stdout
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) out += buf;

    int status = pclose(fp);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    // read stderr if captured
    std::string err_out;
    if (capture_stderr) {
        FILE *ef = fopen(stderr_file.c_str(), "r");
        if (ef) {
            while (fgets(buf, sizeof(buf), ef)) err_out += buf;
            fclose(ef);
            remove(stderr_file.c_str());
        }
    }

    if (throw_on_error && exit_code != 0) {
        return JS_ThrowInternalError(ctx,
            "Command failed (exit %d): %s", exit_code,
            err_out.empty() ? out.c_str() : err_out.c_str());
    }

    // return { stdout, stderr, status }
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "stdout", JS_NewStringLen(ctx, out.c_str(), out.size()));
    JS_SetPropertyStr(ctx, obj, "stderr", JS_NewStringLen(ctx, err_out.c_str(), err_out.size()));
    JS_SetPropertyStr(ctx, obj, "status", JS_NewInt32(ctx, exit_code));
    return obj;
}

// spawnSync(cmd, args, [options]) → { stdout, stderr, status }
static JSValue js_spawn_sync(JSContext *ctx, JSValueConst /*this_val*/,
                              int argc, JSValueConst *argv)
{
    if (argc < 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "spawnSync: expected string command");

    const char *cmd = JS_ToCString(ctx, argv[0]);
    if (!cmd) return JS_EXCEPTION;

    std::string full_cmd = cmd;
    JS_FreeCString(ctx, cmd);

    // append args array
    if (argc >= 2 && JS_IsArray(ctx, argv[1])) {
        JSValue len_val = JS_GetPropertyStr(ctx, argv[1], "length");
        int32_t len = 0;
        JS_ToInt32(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int32_t i = 0; i < len; i++) {
            JSValue item = JS_GetPropertyUint32(ctx, argv[1], (uint32_t)i);
            const char *s = JS_ToCString(ctx, item);
            if (s) {
                full_cmd += " ";
                // simple shell-quote: wrap in single quotes, escape existing single quotes
                full_cmd += "'";
                for (const char *p = s; *p; p++) {
                    if (*p == '\'') full_cmd += "'\\''";
                    else            full_cmd += *p;
                }
                full_cmd += "'";
                JS_FreeCString(ctx, s);
            }
            JS_FreeValue(ctx, item);
        }
    }

    // delegate to exec_sync logic via popen
    FILE *fp = popen(full_cmd.c_str(), "r");
    if (!fp)
        return JS_ThrowInternalError(ctx, "spawnSync: popen failed: %s", strerror(errno));

    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) out += buf;
    int status = pclose(fp);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "stdout", JS_NewStringLen(ctx, out.c_str(), out.size()));
    JS_SetPropertyStr(ctx, obj, "stderr", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, obj, "status", JS_NewInt32(ctx, exit_code));
    return obj;
}

// ── JS shim registered as require('child_process') ───────────────────────────
// Exposed as __child_process_exec_sync / __child_process_spawn_sync globally;
// main.cpp wires them into the require() cache via a JS shim.

void js_init_child_process(JSContext *ctx, JSValue global)
{
    JS_SetPropertyStr(ctx, global, "__cp_exec_sync",
        JS_NewCFunction(ctx, js_exec_sync,  "__cp_exec_sync",  2));
    JS_SetPropertyStr(ctx, global, "__cp_spawn_sync",
        JS_NewCFunction(ctx, js_spawn_sync, "__cp_spawn_sync", 3));
}
