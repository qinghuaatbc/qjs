#include "console.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>

extern "C" {
#include "../quickjs/quickjs.h"
}

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string js_value_to_string(JSContext *ctx, JSValueConst val) {
    int tag = JS_VALUE_GET_TAG(val);

    if (JS_IsUndefined(val)) return "undefined";
    if (JS_IsNull(val))      return "null";
    if (JS_IsBool(val))      return JS_ToBool(ctx, val) ? "true" : "false";

    if (tag == JS_TAG_INT) {
        int64_t i;
        JS_ToInt64(ctx, &i, val);
        return std::to_string(i);
    }

    if (tag == JS_TAG_FLOAT64) {
        double d;
        JS_ToFloat64(ctx, &d, val);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", d);
        // Remove trailing zeros after decimal
        std::string s(buf);
        if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
            s.erase(s.find_last_not_of('0') + 1);
            if (s.back() == '.') s.pop_back();
        }
        return s;
    }

    if (JS_IsFunction(ctx, val)) {
        JSValue name = JS_GetPropertyStr(ctx, val, "name");
        const char *n = JS_ToCString(ctx, name);
        std::string result = "[Function: ";
        result += (n && *n) ? n : "(anonymous)";
        result += "]";
        if (n) JS_FreeCString(ctx, n);
        JS_FreeValue(ctx, name);
        return result;
    }

    if (JS_IsArray(ctx, val)) {
        JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
        int64_t len = 0;
        JS_ToInt64(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        std::string result = "[ ";
        for (int64_t i = 0; i < len; i++) {
            if (i > 0) result += ", ";
            if (i >= 100) { result += "... "; break; }
            JSValue item = JS_GetPropertyUint32(ctx, val, (uint32_t)i);
            result += js_value_to_string(ctx, item);
            JS_FreeValue(ctx, item);
        }
        result += " ]";
        return result;
    }

    if (JS_IsObject(val)) {
        JSPropertyEnum *props = nullptr;
        uint32_t prop_count = 0;
        JS_GetOwnPropertyNames(ctx, &props, &prop_count, val,
                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);
        std::string result = "{ ";
        for (uint32_t i = 0; i < prop_count; i++) {
            if (i > 0) result += ", ";
            const char *key = JS_AtomToCString(ctx, props[i].atom);
            JSValue v = JS_GetProperty(ctx, val, props[i].atom);
            result += key;
            result += ": ";
            result += js_value_to_string(ctx, v);
            JS_FreeCString(ctx, key);
            JS_FreeValue(ctx, v);
            JS_FreeAtom(ctx, props[i].atom);
        }
        if (props) js_free(ctx, props);
        result += " }";
        return result;
    }

    // String and everything else
    const char *str = JS_ToCString(ctx, val);
    std::string result = str ? str : "";
    if (str) JS_FreeCString(ctx, str);
    return result;
}

static std::string format_args(JSContext *ctx, int argc, JSValueConst *argv) {
    std::string out;
    for (int i = 0; i < argc; i++) {
        if (i > 0) out += ' ';
        // If first arg is a format string with %s/%d/%o etc., handle it
        if (i == 0 && JS_IsString(argv[0])) {
            const char *fmt = JS_ToCString(ctx, argv[0]);
            std::string f(fmt ? fmt : "");
            JS_FreeCString(ctx, fmt);

            int arg_idx = 1;
            std::string formatted;
            for (size_t j = 0; j < f.size(); j++) {
                if (f[j] == '%' && j + 1 < f.size() && arg_idx < argc) {
                    char spec = f[j+1];
                    if (spec == 's' || spec == 'd' || spec == 'i' ||
                        spec == 'f' || spec == 'o' || spec == 'O') {
                        formatted += js_value_to_string(ctx, argv[arg_idx++]);
                        j++;
                        continue;
                    }
                }
                formatted += f[j];
            }
            out += formatted;
            // Remaining args
            for (; arg_idx < argc; arg_idx++) {
                out += ' ';
                out += js_value_to_string(ctx, argv[arg_idx]);
            }
            return out;
        }
        out += js_value_to_string(ctx, argv[i]);
    }
    return out;
}

// ── counters and timers ───────────────────────────────────────────────────────
static std::unordered_map<std::string, uint32_t> s_counters;
static std::unordered_map<std::string, clock_t>  s_timers;
static int s_group_depth = 0;

static std::string indent() {
    return std::string(s_group_depth * 2, ' ');
}

// ── console methods ───────────────────────────────────────────────────────────

static JSValue js_console_log(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    printf("%s%s\n", indent().c_str(), format_args(ctx, argc, argv).c_str());
    return JS_UNDEFINED;
}

static JSValue js_console_error(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    fprintf(stderr, "%s\033[31m%s\033[0m\n", indent().c_str(), format_args(ctx, argc, argv).c_str());
    return JS_UNDEFINED;
}

static JSValue js_console_warn(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    fprintf(stderr, "%s\033[33m%s\033[0m\n", indent().c_str(), format_args(ctx, argc, argv).c_str());
    return JS_UNDEFINED;
}

static JSValue js_console_info(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    printf("%s\033[36m%s\033[0m\n", indent().c_str(), format_args(ctx, argc, argv).c_str());
    return JS_UNDEFINED;
}

static JSValue js_console_dir(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc > 0)
        printf("%s%s\n", indent().c_str(), js_value_to_string(ctx, argv[0]).c_str());
    return JS_UNDEFINED;
}

static JSValue js_console_assert(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    bool ok = (argc > 0) && JS_ToBool(ctx, argv[0]);
    if (!ok) {
        std::string msg = "Assertion failed";
        if (argc > 1) {
            msg += ": ";
            msg += format_args(ctx, argc - 1, argv + 1);
        }
        fprintf(stderr, "\033[31m%s%s\033[0m\n", indent().c_str(), msg.c_str());
    }
    return JS_UNDEFINED;
}

static JSValue js_console_time(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    std::string label = "default";
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { label = s; JS_FreeCString(ctx, s); }
    }
    s_timers[label] = clock();
    return JS_UNDEFINED;
}

static JSValue js_console_time_end(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    std::string label = "default";
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { label = s; JS_FreeCString(ctx, s); }
    }
    auto it = s_timers.find(label);
    if (it != s_timers.end()) {
        double ms = (double)(clock() - it->second) / CLOCKS_PER_SEC * 1000.0;
        printf("%s%s: %.3f ms\n", indent().c_str(), label.c_str(), ms);
        s_timers.erase(it);
    } else {
        fprintf(stderr, "Timer '%s' does not exist\n", label.c_str());
    }
    return JS_UNDEFINED;
}

static JSValue js_console_count(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    std::string label = "default";
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { label = s; JS_FreeCString(ctx, s); }
    }
    uint32_t cnt = ++s_counters[label];
    printf("%s%s: %u\n", indent().c_str(), label.c_str(), cnt);
    return JS_UNDEFINED;
}

static JSValue js_console_count_reset(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    std::string label = "default";
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { label = s; JS_FreeCString(ctx, s); }
    }
    s_counters[label] = 0;
    return JS_UNDEFINED;
}

static JSValue js_console_group(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc > 0)
        printf("%s%s\n", indent().c_str(), format_args(ctx, argc, argv).c_str());
    s_group_depth++;
    return JS_UNDEFINED;
}

static JSValue js_console_group_end(JSContext *, JSValueConst, int, JSValueConst *) {
    if (s_group_depth > 0) s_group_depth--;
    return JS_UNDEFINED;
}

static JSValue js_console_clear(JSContext *, JSValueConst, int, JSValueConst *) {
    printf("\033[2J\033[H");
    return JS_UNDEFINED;
}

static JSValue js_console_table(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc > 0)
        printf("%s%s\n", indent().c_str(), js_value_to_string(ctx, argv[0]).c_str());
    return JS_UNDEFINED;
}

// ── install ───────────────────────────────────────────────────────────────────

#define SET_FN(obj, name, fn, len) \
    JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, fn, name, len))

void js_init_console(JSContext *ctx, JSValue global) {
    JSValue console = JS_NewObject(ctx);

    SET_FN(console, "log",        js_console_log,        1);
    SET_FN(console, "error",      js_console_error,      1);
    SET_FN(console, "warn",       js_console_warn,       1);
    SET_FN(console, "info",       js_console_info,       1);
    SET_FN(console, "dir",        js_console_dir,        1);
    SET_FN(console, "assert",     js_console_assert,     1);
    SET_FN(console, "time",       js_console_time,       1);
    SET_FN(console, "timeEnd",    js_console_time_end,   1);
    SET_FN(console, "count",      js_console_count,      0);
    SET_FN(console, "countReset", js_console_count_reset,0);
    SET_FN(console, "group",      js_console_group,      0);
    SET_FN(console, "groupEnd",   js_console_group_end,  0);
    SET_FN(console, "groupCollapsed", js_console_group,  0);
    SET_FN(console, "clear",      js_console_clear,      0);
    SET_FN(console, "table",      js_console_table,      1);
    SET_FN(console, "debug",      js_console_log,        1);
    SET_FN(console, "trace",      js_console_log,        1);

    JS_SetPropertyStr(ctx, global, "console", console);
}
