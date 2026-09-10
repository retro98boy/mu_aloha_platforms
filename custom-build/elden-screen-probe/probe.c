// SPDX-License-Identifier: BSD-2-Clause-Patent
// Isolated bootloader-to-screen experiment. No EFI, PMIC, MMIO or storage APIs.
#include <stdint.h>
#include <stddef.h>

#define SPLASH_BASE 0xfc800000ULL
#define SPLASH_SIZE 0x02b00000ULL
#define WIDTH 1904U
#define HEIGHT 3040U

static uint32_t be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static int equal(const unsigned char *p, size_t n, const char *s)
{
    size_t i = 0;
    while (s[i]) {
        if (i >= n || p[i] != (unsigned char)s[i]) return 0;
        i++;
    }
    return i < n && p[i] == 0;
}

static int selected_panel(const unsigned char *p, size_t n)
{
    const char *s = "msm_drm.dsi_display0=qcom,mdss_dsi_elden_nt36523_csot_3k_dsc_vid:";
    size_t i, j;
    if (!n || p[n - 1] != 0) return 0;
    for (i = 0; i < n; i++) {
        if (i && p[i - 1] != ' ') continue;
        for (j = 0; s[j] && i + j < n && p[i + j] == (unsigned char)s[j]; j++) {}
        if (!s[j]) return 1;
    }
    return 0;
}

// Bounded parser, no allocation and no libfdt/UEFI dependencies. Reject duplicate
// target nodes/properties, truncation, unsupported cell widths and malformed tags.
int probe_fdt_matches(const unsigned char *b, size_t available)
{
    uint32_t total, start, strings, struct_size, string_size, pos, end;
    unsigned depth = 0, paths[32];
    unsigned seen_root = 0;
    unsigned panel = 0, splash = 0, cells_a = 0, cells_s = 0;
    unsigned seen_chosen = 0, seen_reserved = 0, seen_splash = 0, seen_bootargs = 0;
    if (available < 40 || be32(b) != 0xd00dfeed) return 0;
    total = be32(b + 4); start = be32(b + 8); strings = be32(b + 12);
    string_size = be32(b + 32); struct_size = be32(b + 36);
    if (total > available || total < 40 || total > 0x200000 ||
        be32(b + 20) != 17 || be32(b + 24) > 17 ||
        start < 40 || start > total || (start & 3) || struct_size > total - start ||
        strings < 40 || strings > total || string_size > total - strings) return 0;
    if (start < strings + string_size && strings < start + struct_size) return 0;
    pos = start; end = start + struct_size;
    while (pos <= end && end - pos >= 4) {
        uint32_t tag = be32(b + pos); pos += 4;
        if (tag == 1) {
            uint32_t first = pos, kind = 0, parent = depth ? paths[depth - 1] : 0;
            while (pos < end && b[pos]) pos++;
            if (pos == end || depth == 32) return 0;
            if (depth == 0) {
                if (first != pos || seen_root++) return 0;
                kind = 1;
            } else if (depth == 1 && parent == 1) {
                if (equal(b + first, pos - first + 1, "chosen")) {
                    if (seen_chosen++) return 0;
                    kind = 2;
                }
                if (equal(b + first, pos - first + 1, "reserved-memory")) {
                    if (seen_reserved++) return 0;
                    kind = 3;
                }
            } else if (depth == 2 && parent == 3 &&
                       equal(b + first, pos - first + 1, "splash_region")) {
                if (seen_splash++) return 0;
                kind = 4;
            }
            paths[depth++] = kind;
            pos = (pos + 4) & ~3U;
            if (pos > end) return 0;
        } else if (tag == 2) {
            if (!depth) return 0;
            depth--;
        } else if (tag == 3) {
            uint32_t length, name, kind;
            const unsigned char *key, *value;
            if (!depth || end - pos < 8) return 0;
            length = be32(b + pos); name = be32(b + pos + 4); pos += 8;
            if (name >= string_size || length > end - pos) return 0;
            key = b + strings + name; value = b + pos; kind = paths[depth - 1];
            if (kind == 2 && equal(key, string_size - name, "bootargs")) {
                if (seen_bootargs++) return 0;
                panel = selected_panel(value, length);
            }
            if (kind == 3 && equal(key, string_size - name, "#address-cells")) {
                if (cells_a || length != 4 || be32(value) != 2) return 0;
                cells_a = 1;
            }
            if (kind == 3 && equal(key, string_size - name, "#size-cells")) {
                if (cells_s || length != 4 || be32(value) != 2) return 0;
                cells_s = 1;
            }
            if (kind == 4 && equal(key, string_size - name, "reg")) {
                if (splash || length != 16 || be32(value) || be32(value + 8) ||
                    be32(value + 4) != SPLASH_BASE || be32(value + 12) != SPLASH_SIZE) return 0;
                splash = 1;
            }
            pos += (length + 3) & ~3U;
            if (pos > end) return 0;
        } else if (tag == 4) {
            continue;
        } else if (tag == 9) {
            return depth == 0 && panel && splash && cells_a && cells_s;
        } else return 0;
    }
    return 0;
}

// A 256x64 black/white checker at (64,64). BGRA32/stride=1904*4 is an
// evidence-backed TEST hypothesis, not a measurement of the running GOP.
// Limit writes to this small rectangle; do not clear the display reservation.
void probe_draw(volatile uint32_t *frame)
{
    unsigned x, y;
    for (y = 64; y < 128; y++)
        for (x = 64; x < 320; x++)
            frame[y * WIDTH + x] = ((x / 16 + y / 16) & 1) ? 0xffffffffU : 0xff000000U;
}

void probe_main(const unsigned char *dtb)
{
    uintptr_t address = (uintptr_t)dtb;
    // Expected ABL kernel/DTB arena; do not dereference arbitrary x0.
    if (address < 0xa8000000ULL || address > 0xb8000000ULL - 40 || (address & 7)) return;
    if (!probe_fdt_matches(dtb, 0xb8000000ULL - address)) return;
    if ((uint64_t)WIDTH * HEIGHT * 4 > SPLASH_SIZE) return;
    probe_draw((volatile uint32_t *)(uintptr_t)SPLASH_BASE);
#if defined(__aarch64__)
    __asm__ volatile("dsb sy" ::: "memory");
#endif
}
