/*
 * Minimal subset of quickjs-libc for platforms that cannot build the full
 * std/os modules (e.g. PS Vita: no termios, fork or signals).
 *
 * Only the helpers used by LibreSprite's QuickJS interpreter are provided.
 * Released under the same MIT license as QuickJS.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "quickjs.h"
#include "quickjs-libc.h"

void js_std_init_handlers(JSRuntime *rt)
{
    (void)rt;
}

void js_std_free_handlers(JSRuntime *rt)
{
    (void)rt;
}

static void dump_value(JSContext *ctx, JSValueConst val)
{
    const char *str = JS_ToCString(ctx, val);
    if (str) {
        fprintf(stderr, "%s\n", str);
        JS_FreeCString(ctx, str);
    } else {
        fprintf(stderr, "[exception]\n");
    }
}

void js_std_dump_error(JSContext *ctx)
{
    JSValue exception_val = JS_GetException(ctx);
    dump_value(ctx, exception_val);
    if (JS_IsError(exception_val)) {
        JSValue stack = JS_GetPropertyStr(ctx, exception_val, "stack");
        if (!JS_IsUndefined(stack))
            dump_value(ctx, stack);
        JS_FreeValue(ctx, stack);
    }
    JS_FreeValue(ctx, exception_val);
}

/* Runs pending promise jobs. There are no timers or I/O handlers here, so
   the loop ends as soon as the job queue is empty. */
int js_std_loop(JSContext *ctx)
{
    JSContext *ctx1;
    for (;;) {
        int err = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);
        if (err <= 0)
            break;
    }
    return JS_HasException(ctx);
}

int js_module_set_import_meta(JSContext *ctx, JSValueConst func_val,
                              bool use_realpath, bool is_main)
{
    JSModuleDef *m;
    char buf[1024 + 16];
    JSValue meta_obj;
    JSAtom module_name_atom;
    const char *module_name;

    (void)use_realpath;
    if (JS_VALUE_GET_TAG(func_val) != JS_TAG_MODULE)
        return -1;
    m = JS_VALUE_GET_PTR(func_val);

    module_name_atom = JS_GetModuleName(ctx, m);
    module_name = JS_AtomToCString(ctx, module_name_atom);
    JS_FreeAtom(ctx, module_name_atom);
    if (!module_name)
        return -1;
    if (!strchr(module_name, ':'))
        snprintf(buf, sizeof(buf), "file://%s", module_name);
    else
        snprintf(buf, sizeof(buf), "%s", module_name);
    JS_FreeCString(ctx, module_name);

    meta_obj = JS_GetImportMeta(ctx, m);
    if (JS_IsException(meta_obj))
        return -1;
    JS_DefinePropertyValueStr(ctx, meta_obj, "url",
                              JS_NewString(ctx, buf),
                              JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, meta_obj, "main",
                              JS_NewBool(ctx, is_main),
                              JS_PROP_C_W_E);
    JS_FreeValue(ctx, meta_obj);
    return 0;
}
