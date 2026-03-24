#pragma once
extern "C" {
#include "../quickjs/quickjs.h"
}

void js_init_process(JSContext *ctx, JSValue global, int argc, char **argv);
