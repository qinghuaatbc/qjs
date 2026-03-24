#pragma once
extern "C" {
#include "../quickjs/quickjs.h"
}

void js_init_http(JSContext *ctx, JSValue global);
