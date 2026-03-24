#pragma once
extern "C" {
#include "../quickjs/quickjs.h"
}

void js_init_net(JSContext *ctx, JSValue global);
