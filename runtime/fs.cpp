#include "fs.hpp"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

extern "C" {
#include "../quickjs/quickjs.h"
}

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string read_file(const char *path, bool *ok) {
    FILE *f = fopen(path, "rb");
    if (!f) { *ok = false; return strerror(errno); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    std::string buf(size, '\0');
    fread(&buf[0], 1, size, f);
    fclose(f);
    *ok = true;
    return buf;
}

// ── fs functions ──────────────────────────────────────────────────────────────

static JSValue js_read_file_sync(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "readFileSync requires a path");
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    // options: 'utf8' string or {encoding:'utf8'}
    bool as_string = false;
    if (argc > 1) {
        if (JS_IsString(argv[1])) {
            as_string = true;
        } else if (JS_IsObject(argv[1])) {
            JSValue enc = JS_GetPropertyStr(ctx, argv[1], "encoding");
            if (!JS_IsUndefined(enc)) as_string = true;
            JS_FreeValue(ctx, enc);
        }
    }

    bool ok;
    std::string data = read_file(path, &ok);
    JS_FreeCString(ctx, path);

    if (!ok) return JS_ThrowInternalError(ctx, "ENOENT: %s", data.c_str());

    if (as_string)
        return JS_NewStringLen(ctx, data.c_str(), data.size());

    // Return Buffer-like Uint8Array
    JSValue buf = JS_NewArrayBufferCopy(ctx,
        (const uint8_t *)data.data(), data.size());
    return buf;
}

static JSValue js_write_file_sync(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "writeFileSync requires path and data");
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    FILE *f = fopen(path, "wb");
    if (!f) {
        JS_FreeCString(ctx, path);
        return JS_ThrowInternalError(ctx, "Cannot open file for writing: %s", strerror(errno));
    }

    if (JS_IsString(argv[1])) {
        const char *data = JS_ToCString(ctx, argv[1]);
        if (data) { fwrite(data, 1, strlen(data), f); JS_FreeCString(ctx, data); }
    } else {
        size_t size;
        uint8_t *data = JS_GetArrayBuffer(ctx, &size, argv[1]);
        if (data) fwrite(data, 1, size, f);
    }

    fclose(f);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static JSValue js_exists_sync(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_FALSE;
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_FALSE;
    struct stat st;
    bool exists = (stat(path, &st) == 0);
    JS_FreeCString(ctx, path);
    return JS_NewBool(ctx, exists);
}

static JSValue js_mkdir_sync(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "mkdirSync requires a path");
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    // Check for {recursive: true} option
    bool recursive = false;
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue r = JS_GetPropertyStr(ctx, argv[1], "recursive");
        recursive = JS_ToBool(ctx, r);
        JS_FreeValue(ctx, r);
    }

    int ret;
    if (recursive) {
        // Create each component
        std::string p(path);
        for (size_t i = 1; i <= p.size(); i++) {
            if (i == p.size() || p[i] == '/') {
                mkdir(p.substr(0, i).c_str(), 0755);
            }
        }
        ret = 0;
    } else {
        ret = mkdir(path, 0755);
    }

    JS_FreeCString(ctx, path);
    if (ret != 0 && errno != EEXIST)
        return JS_ThrowInternalError(ctx, "mkdir failed: %s", strerror(errno));
    return JS_UNDEFINED;
}

static JSValue js_readdir_sync(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "readdirSync requires a path");
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    DIR *dir = opendir(path);
    JS_FreeCString(ctx, path);
    if (!dir) return JS_ThrowInternalError(ctx, "Cannot open directory: %s", strerror(errno));

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, entry->d_name));
    }
    closedir(dir);
    return arr;
}

static JSValue js_stat_sync(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "statSync requires a path");
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    struct stat st;
    int ret = stat(path, &st);
    JS_FreeCString(ctx, path);

    if (ret != 0) return JS_ThrowInternalError(ctx, "stat failed: %s", strerror(errno));

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "size",  JS_NewInt64(ctx, st.st_size));
    JS_SetPropertyStr(ctx, obj, "isFile",
        JS_NewCFunction(ctx, [](JSContext *c, JSValueConst this_val, int, JSValueConst *) -> JSValue {
            JSValue isFile = JS_GetPropertyStr(c, this_val, "_isFile");
            JSValue ret = JS_NewBool(c, JS_ToBool(c, isFile));
            JS_FreeValue(c, isFile);
            return ret;
        }, "isFile", 0));
    JS_SetPropertyStr(ctx, obj, "isDirectory",
        JS_NewCFunction(ctx, [](JSContext *c, JSValueConst this_val, int, JSValueConst *) -> JSValue {
            JSValue isDir = JS_GetPropertyStr(c, this_val, "_isDir");
            JSValue ret = JS_NewBool(c, JS_ToBool(c, isDir));
            JS_FreeValue(c, isDir);
            return ret;
        }, "isDirectory", 0));
    JS_SetPropertyStr(ctx, obj, "_isFile",  JS_NewBool(ctx, S_ISREG(st.st_mode)));
    JS_SetPropertyStr(ctx, obj, "_isDir",   JS_NewBool(ctx, S_ISDIR(st.st_mode)));
    JS_SetPropertyStr(ctx, obj, "mtime",    JS_NewInt64(ctx, (int64_t)st.st_mtime * 1000));
    return obj;
}

static JSValue js_unlink_sync(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "unlinkSync requires a path");
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;
    int ret = unlink(path);
    JS_FreeCString(ctx, path);
    if (ret != 0) return JS_ThrowInternalError(ctx, "unlink failed: %s", strerror(errno));
    return JS_UNDEFINED;
}

static JSValue js_rename_sync(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "renameSync requires oldPath and newPath");
    const char *from = JS_ToCString(ctx, argv[0]);
    const char *to   = JS_ToCString(ctx, argv[1]);
    int ret = (from && to) ? rename(from, to) : -1;
    if (from) JS_FreeCString(ctx, from);
    if (to)   JS_FreeCString(ctx, to);
    if (ret != 0) return JS_ThrowInternalError(ctx, "rename failed: %s", strerror(errno));
    return JS_UNDEFINED;
}

// ── install ───────────────────────────────────────────────────────────────────

void js_init_fs(JSContext *ctx, JSValue global) {
    JSValue fs = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, fs, "readFileSync",
        JS_NewCFunction(ctx, js_read_file_sync, "readFileSync", 2));
    JS_SetPropertyStr(ctx, fs, "writeFileSync",
        JS_NewCFunction(ctx, js_write_file_sync, "writeFileSync", 2));
    JS_SetPropertyStr(ctx, fs, "existsSync",
        JS_NewCFunction(ctx, js_exists_sync, "existsSync", 1));
    JS_SetPropertyStr(ctx, fs, "mkdirSync",
        JS_NewCFunction(ctx, js_mkdir_sync, "mkdirSync", 1));
    JS_SetPropertyStr(ctx, fs, "readdirSync",
        JS_NewCFunction(ctx, js_readdir_sync, "readdirSync", 1));
    JS_SetPropertyStr(ctx, fs, "statSync",
        JS_NewCFunction(ctx, js_stat_sync, "statSync", 1));
    JS_SetPropertyStr(ctx, fs, "unlinkSync",
        JS_NewCFunction(ctx, js_unlink_sync, "unlinkSync", 1));
    JS_SetPropertyStr(ctx, fs, "renameSync",
        JS_NewCFunction(ctx, js_rename_sync, "renameSync", 2));

    // Expose as both 'fs' global and via require('fs') shimming
    JS_SetPropertyStr(ctx, global, "__fs", fs);
}
