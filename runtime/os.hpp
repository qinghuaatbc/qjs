#pragma once
extern "C" {
#include "../quickjs/quickjs.h"
}

void js_init_os_module(JSContext *ctx, JSValue global);
