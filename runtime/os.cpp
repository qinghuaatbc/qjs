#include "os.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <unistd.h>
#include <sys/utsname.h>
#include <pwd.h>

#ifdef __APPLE__
#  include <sys/sysctl.h>
#  include <mach/mach.h>
#  include <mach/mach_host.h>
#  include <sys/types.h>
#  include <time.h>
#else
#  include <sys/sysinfo.h>
#endif

extern "C" {
#include "../quickjs/quickjs.h"
}

// ── __os_hostname() ───────────────────────────────────────────────────────────
static JSValue js_os_hostname(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    char buf[256] = {};
    gethostname(buf, sizeof(buf) - 1);
    return JS_NewString(ctx, buf);
}

// ── __os_homedir() ────────────────────────────────────────────────────────────
static JSValue js_os_homedir(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    const char *home = getenv("HOME");
    if (home && home[0]) return JS_NewString(ctx, home);
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return JS_NewString(ctx, pw->pw_dir);
    return JS_NewString(ctx, "/");
}

// ── __os_tmpdir() ─────────────────────────────────────────────────────────────
static JSValue js_os_tmpdir(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    const char *tmp = getenv("TMPDIR");
    if (tmp && tmp[0]) return JS_NewString(ctx, tmp);
    return JS_NewString(ctx, "/tmp");
}

// ── __os_uptime() ─────────────────────────────────────────────────────────────
static JSValue js_os_uptime(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    double uptime = 0.0;
#ifdef __APPLE__
    struct timeval boottime{};
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if (sysctl(mib, 2, &boottime, &len, nullptr, 0) == 0) {
        struct timeval now{};
        gettimeofday(&now, nullptr);
        uptime = (double)(now.tv_sec - boottime.tv_sec)
               + (double)(now.tv_usec - boottime.tv_usec) / 1e6;
    }
#else
    struct sysinfo info{};
    if (sysinfo(&info) == 0) uptime = (double)info.uptime;
#endif
    return JS_NewFloat64(ctx, uptime);
}

// ── __os_freemem() ────────────────────────────────────────────────────────────
static JSValue js_os_freemem(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    uint64_t free_mem = 0;
#ifdef __APPLE__
    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vmstat{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(host, HOST_VM_INFO64,
                          (host_info64_t)&vmstat, &count) == KERN_SUCCESS) {
        vm_size_t page_size = 0;
        host_page_size(host, &page_size);
        free_mem = (uint64_t)(vmstat.free_count + vmstat.inactive_count) * page_size;
    }
#else
    struct sysinfo info{};
    if (sysinfo(&info) == 0)
        free_mem = (uint64_t)info.freeram * info.mem_unit;
#endif
    return JS_NewFloat64(ctx, (double)free_mem);
}

// ── __os_totalmem() ───────────────────────────────────────────────────────────
static JSValue js_os_totalmem(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    uint64_t total = 0;
#ifdef __APPLE__
    size_t len = sizeof(total);
    sysctlbyname("hw.memsize", &total, &len, nullptr, 0);
#else
    struct sysinfo info{};
    if (sysinfo(&info) == 0)
        total = (uint64_t)info.totalram * info.mem_unit;
#endif
    return JS_NewFloat64(ctx, (double)total);
}

// ── __os_cpus() ───────────────────────────────────────────────────────────────
static JSValue js_os_cpus(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    int nproc = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc <= 0) nproc = 1;

    std::string model = "Unknown";
    int speed = 0;

#ifdef __APPLE__
    char brand[256] = {};
    size_t len = sizeof(brand);
    sysctlbyname("machdep.cpu.brand_string", brand, &len, nullptr, 0);
    if (brand[0]) model = brand;

    uint64_t freq = 0;
    size_t flen = sizeof(freq);
    if (sysctlbyname("hw.cpufrequency", &freq, &flen, nullptr, 0) == 0) {
        speed = (int)(freq / 1000000);
    } else {
        // On Apple Silicon hw.cpufrequency may not exist
        int64_t nominal = 0;
        size_t nlen = sizeof(nominal);
        if (sysctlbyname("hw.cpufrequency_max", &nominal, &nlen, nullptr, 0) == 0)
            speed = (int)(nominal / 1000000);
    }
#endif

    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < nproc; i++) {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "model", JS_NewString(ctx, model.c_str()));
        JS_SetPropertyStr(ctx, obj, "speed", JS_NewInt32(ctx, speed));
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, obj);
    }
    return arr;
}

// ── __os_arch() ───────────────────────────────────────────────────────────────
static JSValue js_os_arch(JSContext *ctx, JSValueConst, int, JSValueConst *) {
#if defined(__aarch64__) || defined(__arm64__)
    return JS_NewString(ctx, "arm64");
#elif defined(__x86_64__) || defined(__amd64__)
    return JS_NewString(ctx, "x64");
#elif defined(__i386__)
    return JS_NewString(ctx, "ia32");
#elif defined(__arm__)
    return JS_NewString(ctx, "arm");
#else
    struct utsname u{};
    uname(&u);
    return JS_NewString(ctx, u.machine);
#endif
}

// ── __os_platform() ───────────────────────────────────────────────────────────
static JSValue js_os_platform(JSContext *ctx, JSValueConst, int, JSValueConst *) {
#ifdef __APPLE__
    return JS_NewString(ctx, "darwin");
#elif defined(__linux__)
    return JS_NewString(ctx, "linux");
#elif defined(_WIN32)
    return JS_NewString(ctx, "win32");
#else
    return JS_NewString(ctx, "unknown");
#endif
}

// ── __os_network_interfaces() ─────────────────────────────────────────────────
// Stub: returns empty object (full implementation requires ifaddrs.h parsing)
static JSValue js_os_network_interfaces(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    return JS_NewObject(ctx);
}

// ── install ───────────────────────────────────────────────────────────────────
void js_init_os_module(JSContext *ctx, JSValue global) {
    JS_SetPropertyStr(ctx, global, "__os_hostname",
        JS_NewCFunction(ctx, js_os_hostname, "__os_hostname", 0));
    JS_SetPropertyStr(ctx, global, "__os_homedir",
        JS_NewCFunction(ctx, js_os_homedir, "__os_homedir", 0));
    JS_SetPropertyStr(ctx, global, "__os_tmpdir",
        JS_NewCFunction(ctx, js_os_tmpdir, "__os_tmpdir", 0));
    JS_SetPropertyStr(ctx, global, "__os_uptime",
        JS_NewCFunction(ctx, js_os_uptime, "__os_uptime", 0));
    JS_SetPropertyStr(ctx, global, "__os_freemem",
        JS_NewCFunction(ctx, js_os_freemem, "__os_freemem", 0));
    JS_SetPropertyStr(ctx, global, "__os_totalmem",
        JS_NewCFunction(ctx, js_os_totalmem, "__os_totalmem", 0));
    JS_SetPropertyStr(ctx, global, "__os_cpus",
        JS_NewCFunction(ctx, js_os_cpus, "__os_cpus", 0));
    JS_SetPropertyStr(ctx, global, "__os_arch",
        JS_NewCFunction(ctx, js_os_arch, "__os_arch", 0));
    JS_SetPropertyStr(ctx, global, "__os_platform",
        JS_NewCFunction(ctx, js_os_platform, "__os_platform", 0));
    JS_SetPropertyStr(ctx, global, "__os_network_interfaces",
        JS_NewCFunction(ctx, js_os_network_interfaces, "__os_network_interfaces", 0));
}
