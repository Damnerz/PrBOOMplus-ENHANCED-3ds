#ifndef __GL_SWIZZLE_H__
#define __GL_SWIZZLE_H__

#include <3ds.h>

// MortonInterleave() and GetMortonOffset() borrowed from Citra:
// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version

static const u32 MortonInterleave(u32 x, u32 y) {
    const u32 xlut[] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15};
    const u32 ylut[] = {0x00, 0x02, 0x08, 0x0a, 0x20, 0x22, 0x28, 0x2a};
    return xlut[x % 8] + ylut[y % 8];
}

static inline u32 GetMortonOffset(u32 x, u32 y, u32 bytes_per_pixel) {
    const unsigned int block_height = 8;
    const unsigned int coarse_x = x & ~7;

    u32 i = MortonInterleave(x, y);

    const unsigned int offset = coarse_x * block_height;

    return (i + offset) * bytes_per_pixel;
}

static u32 GetPixelOffset(u32 x, u32 y, u32 width, u32 height) {
    y = height - 1 - y;
    u32 coarse_y = y & ~7;
    return GetMortonOffset(x, y, 4) + coarse_y * width * 4;
}

static void SwizzleTexBufferRGBA8(u32 *in, u32 *out, u32 w, u32 h) {
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            *((u32*)((u8*)out + GetPixelOffset(x, y, w, h))) = in[(y * w) + x];
        }
    }
}

#endif
