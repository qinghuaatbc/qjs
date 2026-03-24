#pragma once
extern "C" {
#include "../quickjs/quickjs.h"
}

void js_init_fs(JSContext *ctx, JSValue global);
