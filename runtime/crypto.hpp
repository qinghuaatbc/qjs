#pragma once
extern "C" {
#include "../quickjs/quickjs.h"
}

void js_init_crypto(JSContext *ctx, JSValue global);
