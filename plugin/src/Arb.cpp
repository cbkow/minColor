/* Arbitrary-data plumbing for MinColorArb: flat struct, memcpy flatten, text PRINT/SCAN. */
#include "MinColorCST.h"
#include <cstring>
#include <cstdio>

#include <atomic>
static std::atomic<uint32_t> g_arbNonce{0x40000000u};
static void ArbDefaults(MinColorArb *a) {
    memset(a, 0, sizeof(*a));
    a->magic = MINC_ARB_MAGIC;
    a->version = MINC_ARB_VERSION;
    a->direction = MINC_DIR_TO_WORKING;
    a->instanceId = g_arbNonce.fetch_add(1);   /* unique per instance: identical comps can never
                                                  hash identically -> no cross-instance cache reuse */
}

static PF_Err NewArb(PF_InData *in_data, PF_ArbitraryH *arbPH) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    *arbPH = suites.HandleSuite1()->host_new_handle(sizeof(MinColorArb));
    if (!*arbPH) return PF_Err_OUT_OF_MEMORY;
    MinColorArb *a = reinterpret_cast<MinColorArb*>(suites.HandleSuite1()->host_lock_handle(*arbPH));
    ArbDefaults(a);
    suites.HandleSuite1()->host_unlock_handle(*arbPH);
    return PF_Err_NONE;
}

PF_Err MincArbNewDefault(PF_InData *in_data, PF_ArbitraryH *arbPH) { return NewArb(in_data, arbPH); }

PF_Err MincHandleArbitrary(PF_InData *in_data, PF_OutData *out_data,
                           PF_ParamDef *params[], PF_LayerDef *output,
                           PF_ArbParamsExtra *extra) {
    PF_Err err = PF_Err_NONE;
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    PF_HandleSuite1 *hs = suites.HandleSuite1();
    MincDebugLog("arb: fn=%d", (int)extra->which_function);
    switch (extra->which_function) {
        case PF_Arbitrary_NEW_FUNC:
            err = NewArb(in_data, extra->u.new_func_params.arbPH);
            break;
        case PF_Arbitrary_DISPOSE_FUNC:
            if (extra->u.dispose_func_params.arbH) hs->host_dispose_handle(extra->u.dispose_func_params.arbH);
            break;
        case PF_Arbitrary_COPY_FUNC: {
            err = NewArb(in_data, extra->u.copy_func_params.dst_arbPH);
            if (!err && extra->u.copy_func_params.src_arbH) {
                void *src = hs->host_lock_handle(extra->u.copy_func_params.src_arbH);
                MincDebugLog("arb: COPY space='%s'", ((const MinColorArb*)src)->space);
                void *dst = hs->host_lock_handle(*extra->u.copy_func_params.dst_arbPH);
                memcpy(dst, src, sizeof(MinColorArb));
                hs->host_unlock_handle(extra->u.copy_func_params.src_arbH);
                hs->host_unlock_handle(*extra->u.copy_func_params.dst_arbPH);
            }
            break;
        }
        case PF_Arbitrary_FLAT_SIZE_FUNC:
            *extra->u.flat_size_func_params.flat_data_sizePLu = sizeof(MinColorArb);
            break;
        case PF_Arbitrary_FLATTEN_FUNC: {
            void *src = hs->host_lock_handle(extra->u.flatten_func_params.arbH);
            memcpy(extra->u.flatten_func_params.flat_dataPV, src, sizeof(MinColorArb));
            MincDebugLog("arb: FLATTEN space='%s'", ((const MinColorArb*)src)->space);
            hs->host_unlock_handle(extra->u.flatten_func_params.arbH);
            break;
        }
        case PF_Arbitrary_UNFLATTEN_FUNC: {
            err = NewArb(in_data, extra->u.unflatten_func_params.arbPH);
            if (!err) {
                MinColorArb *dst = reinterpret_cast<MinColorArb*>(hs->host_lock_handle(*extra->u.unflatten_func_params.arbPH));
                const MinColorArb *src = reinterpret_cast<const MinColorArb*>(extra->u.unflatten_func_params.flat_dataPV);
                MincDebugLog("arb: UNFLATTEN buf=%lu need=%lu magic=%08x",
                             (unsigned long)extra->u.unflatten_func_params.buf_sizeLu,
                             (unsigned long)sizeof(MinColorArb), src->magic);
                if (extra->u.unflatten_func_params.buf_sizeLu >= sizeof(MinColorArb) &&
                    src->magic == MINC_ARB_MAGIC && src->version == MINC_ARB_VERSION) {
                    memcpy(dst, src, sizeof(MinColorArb));
                    dst->space[MINC_SPACE_LEN - 1] = '\0';
                    MincDebugLog("arb: UNFLATTEN ok space='%s'", dst->space);
                } else MincDebugLog("arb: UNFLATTEN REJECTED");
                hs->host_unlock_handle(*extra->u.unflatten_func_params.arbPH);
            }
            break;
        }
        case PF_Arbitrary_INTERP_FUNC:   /* not keyframable in practice: snap to left */
            err = NewArb(in_data, extra->u.interp_func_params.interpPH);
            if (!err && extra->u.interp_func_params.left_arbH) {
                void *src = hs->host_lock_handle(extra->u.interp_func_params.left_arbH);
                void *dst = hs->host_lock_handle(*extra->u.interp_func_params.interpPH);
                memcpy(dst, src, sizeof(MinColorArb));
                hs->host_unlock_handle(extra->u.interp_func_params.left_arbH);
                hs->host_unlock_handle(*extra->u.interp_func_params.interpPH);
            }
            break;
        case PF_Arbitrary_COMPARE_FUNC: {
            const void *a = hs->host_lock_handle(extra->u.compare_func_params.a_arbH);
            const void *b = hs->host_lock_handle(extra->u.compare_func_params.b_arbH);
            *extra->u.compare_func_params.compareP =
                (memcmp(a, b, sizeof(MinColorArb)) == 0) ? PF_ArbCompare_EQUAL : PF_ArbCompare_NOT_EQUAL;
            hs->host_unlock_handle(extra->u.compare_func_params.a_arbH);
            hs->host_unlock_handle(extra->u.compare_func_params.b_arbH);
            break;
        }
        case PF_Arbitrary_PRINT_SIZE_FUNC:
            *extra->u.print_size_func_params.print_sizePLu = MINC_SPACE_LEN + 32;
            break;
        case PF_Arbitrary_PRINT_FUNC: {
            const MinColorArb *a = reinterpret_cast<const MinColorArb*>(hs->host_lock_handle(extra->u.print_func_params.arbH));
            snprintf(extra->u.print_func_params.print_bufferPC, MINC_SPACE_LEN + 32, "%s:%s",
                     a->direction == MINC_DIR_LOOK ? "look" :
                     a->direction == MINC_DIR_TO_WORKING ? "to_working" : "from_working", a->space);
            hs->host_unlock_handle(extra->u.print_func_params.arbH);
            break;
        }
        case PF_Arbitrary_SCAN_FUNC: {
            err = NewArb(in_data, extra->u.scan_func_params.arbPH);
            if (!err) {
                MinColorArb *a = reinterpret_cast<MinColorArb*>(hs->host_lock_handle(*extra->u.scan_func_params.arbPH));
                const char *s = extra->u.scan_func_params.bufPC;
                if (s) {
                    if      (!strncmp(s, "to_working:",   11)) { a->direction = MINC_DIR_TO_WORKING;   strncpy(a->space, s + 11, MINC_SPACE_LEN - 1); }
                    else if (!strncmp(s, "from_working:", 13)) { a->direction = MINC_DIR_FROM_WORKING; strncpy(a->space, s + 13, MINC_SPACE_LEN - 1); }
                    else if (!strncmp(s, "look:",          5)) { a->direction = MINC_DIR_LOOK;         strncpy(a->space, s + 5,  MINC_SPACE_LEN - 1); }
                }
                hs->host_unlock_handle(*extra->u.scan_func_params.arbPH);
            }
            break;
        }
        default: err = PF_Err_UNRECOGNIZED_PARAM_TYPE; break;
    }
    return err;
}
