#include "MincCore.h"
#include <cstring>

void MincUtf16HandleToUtf8(AEGP_SuiteHandler &suites, AEGP_MemHandle h, char *out, size_t outLen) {
    out[0] = '\0';
    if (!h) return;
    A_UTF16Char *p16 = nullptr;
    if (suites.MemorySuite1()->AEGP_LockMemHandle(h, reinterpret_cast<void**>(&p16)) == A_Err_NONE && p16) {
        size_t o = 0;                                   /* BMP-only conversion — config paths/space names are ASCII-ish */
        for (size_t i = 0; p16[i] && o + 4 < outLen; ++i) {
            unsigned c = p16[i];
            if      (c < 0x80)  out[o++] = (char)c;
            else if (c < 0x800) { out[o++] = (char)(0xC0 | (c >> 6));  out[o++] = (char)(0x80 | (c & 0x3F)); }
            else                { out[o++] = (char)(0xE0 | (c >> 12)); out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[o++] = (char)(0x80 | (c & 0x3F)); }
        }
        out[o] = '\0';
        suites.MemorySuite1()->AEGP_UnlockMemHandle(h);
    }
    suites.MemorySuite1()->AEGP_FreeMemHandle(h);
}

void MincU8ToU16(const char *s, A_UTF16Char *out, int cap) {  /* BMP-only, mirrors the reader */
    int o = 0; const unsigned char *p = (const unsigned char *)s;
    while (*p && o < cap - 1) {
        unsigned c = *p++;
        if ((c & 0xE0) == 0xC0 && (*p & 0xC0) == 0x80) c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
        else if ((c & 0xF0) == 0xE0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) {
            c = ((c & 0x0F) << 12) | ((unsigned)(p[0] & 0x3F) << 6) | (unsigned)(p[1] & 0x3F); p += 2;
        }
        out[o++] = (A_UTF16Char)c;
    }
    out[o] = 0;
}
