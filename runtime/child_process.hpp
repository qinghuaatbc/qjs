#pragma once
extern "C" {
#include "../quickjs/quickjs.h"
}

void js_init_child_process(JSContext *ctx, JSValue global);
