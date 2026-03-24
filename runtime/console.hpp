#pragma once
extern "C" {
#include "../quickjs/quickjs.h"
}

void js_init_console(JSContext *ctx, JSValue global);
