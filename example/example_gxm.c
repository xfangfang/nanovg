//
// Copyright (c) 2024 xfangfang xfangfang@126.com
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <stdio.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>

#include "nanovg.h"

#define NANOVG_GXM_IMPLEMENTATION
#define NANOVG_GXM_UTILS_IMPLEMENTATION

#include "nanovg_gxm.h"

#include "demo.h"
#include "perf.h"

#define MOUSE_MOVE 5
#define CLAMP_NUM(a, min, max) { if(a>max) a=max; if(a<min) a=min; }
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define STB_DXT_IMPLEMENTATION

#include "stb_dxt.h"
#include "stb_image.h"

static inline __attribute__((always_inline)) uint32_t nearest_po2(uint32_t val) {
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
    val++;

    return val;
}

static inline __attribute__((always_inline)) uint64_t morton_1(uint64_t x) {
    x = x & 0x5555555555555555;
    x = (x | (x >> 1)) & 0x3333333333333333;
    x = (x | (x >> 2)) & 0x0F0F0F0F0F0F0F0F;
    x = (x | (x >> 4)) & 0x00FF00FF00FF00FF;
    x = (x | (x >> 8)) & 0x0000FFFF0000FFFF;
    x = (x | (x >> 16)) & 0xFFFFFFFFFFFFFFFF;
    return x;
}

static inline __attribute__((always_inline)) void d2xy_morton(uint64_t d, uint64_t *x, uint64_t *y) {
    *x = morton_1(d);
    *y = morton_1(d >> 1);
}

static inline __attribute__((always_inline)) void extract_block(const uint8_t *src, uint32_t width, uint8_t *block) {
    for (int j = 0; j < 4; j++) {
        memcpy(&block[j * 4 * 4], src, 16);
        src += width * 4;
    }
}

/**
 * Compress RGBA data to DXT1 or DXT5
 * Thanks to https://github.com/Rinnegatamante/vitaGL/blob/master/source/utils/gpu_utils.c
 * @param dst DXT data
 * @param src RGBA data
 * @param w source width
 * @param h source height
 * @param stride source stride
 * @param isdxt5 0 for DXT1, others for DXT5
 */

static void dxt_compress_ext(uint8_t *dst, uint8_t *src, uint32_t w, uint32_t h, uint32_t stride, int isdxt5) {
    uint8_t block[64];
    uint32_t align_w = MAX(nearest_po2(w), 64);
    uint32_t align_h = MAX(nearest_po2(h), 64);
    uint32_t s = MIN(align_w, align_h);
    uint32_t num_blocks = s * s / 16;
    const uint32_t block_size = isdxt5 ? 16 : 8;
    uint64_t d, offs_x, offs_y;

    for (d = 0; d < num_blocks; d++, dst += block_size) {
        d2xy_morton(d, &offs_x, &offs_y);
        if (offs_x * 4 >= h || offs_y * 4 >= w)
            continue;
        extract_block(src + offs_y * 16 + offs_x * stride * 16, stride, block);
        stb_compress_dxt_block(dst, block, isdxt5, STB_DXT_NORMAL);
    }
    if (align_w > align_h) {
        return dxt_compress_ext(dst, src + s * 4, w - s, h, stride, isdxt5);
    }
    else if (align_w < align_h) {
        return dxt_compress_ext(dst, src + stride * s * 4, w, h - s, stride, isdxt5);
    }
}

void dxt_compress(uint8_t *dst, uint8_t *src, uint32_t w, uint32_t h, int isdxt5) {
    dxt_compress_ext(dst, src, w, h, w, isdxt5);
}

int loadCompressedImage(NVGcontext *vg, const char *path, int flag) {
    unsigned char *img;
    int w, h, n;
    img = stbi_load(path, &w, &h, &n, 4);
    if (img == NULL) {
        return 0;
    }
    int image = nvgCreateImageRGBA(vg, w, h, flag, NULL);
    NVGXMtexture *texture = nvgxmImageHandle(vg, image);
    dxt_compress(texture->data, img, w, h, flag & NVG_IMAGE_DXT5);
    stbi_image_free(img);
    return image;
}

void renderImage(NVGcontext *vg, int image, float x, float y, float w, float h) {
    NVGpaint imgPaint = nvgImagePattern(vg, x, y, w, h, 0, image, 1);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, imgPaint);
    nvgFill(vg);
    nvgStrokeWidth(vg, 1);
    nvgStrokeColor(vg, nvgRGBAf(1, 1, 0, 1));
    nvgStroke(vg);
}

int main() {
    DemoData data;
    PerfGraph fps, cpuGraph;
    SceCtrlData pad, old_pad;
    int blowup = 0;
    int screenshot = 0;
    int premult = 0;
    unsigned int pressed = 0;
    NVGcontext *vg = NULL;
    double prevt = 0, cpuTime = 0;
    NVGcolor clearColor = nvgRGBAf(0.3f, 0.3f, 0.32f, 1.0f);
    double mx = DISPLAY_WIDTH / 2, my = DISPLAY_HEIGHT / 2, t, dt;

#ifdef USE_VITA_SHARK
    if (shark_init("app0:module/libshacccg.suprx") < 0) {
        sceClibPrintf("vitashark: failed to initialize\n");
        return EXIT_FAILURE;
    }
#endif

    NVGXMwindow *window = NULL;
    NVGXMinitOptions initOptions = {
            .msaa = SCE_GXM_MULTISAMPLE_4X,
            .swapInterval = 0,
            .dumpShader = 1, // Save shaders to ux0:data/nvg_*.c
            .scenesPerFrame = 1,
    };
    window = gxmCreateWindow(&initOptions);
    if (window == NULL) {
        sceClibPrintf("gxm: failed to create window\n");
        return EXIT_FAILURE;
    }

    vg = nvgCreateGXM(window->context, window->shader_patcher, NVG_STENCIL_STROKES);
    if (vg == NULL) {
        sceClibPrintf("nanovg: failed to initialize\n");
        return EXIT_FAILURE;
    }

#ifdef USE_VITA_SHARK
    // Clean up vitashark as we don't need it anymore
    shark_end();
#endif

    if (loadDemoData(vg, &data) == -1)
        return EXIT_FAILURE;

    initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");
    initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");

    memset(&pad, 0, sizeof(pad));
    memset(&old_pad, 0, sizeof(old_pad));
    gxmClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

    int compressedImage = loadCompressedImage(vg, "app0:/example/images/image1.jpg", NVG_IMAGE_DXT1);

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        pressed = pad.buttons & ~old_pad.buttons;
        old_pad = pad;

        if (pressed & SCE_CTRL_START)
            break;
        if (pressed & SCE_CTRL_TRIANGLE)
            blowup = !blowup;
        if (pad.buttons & SCE_CTRL_CROSS)
            screenshot = 1;
        if (pressed & SCE_CTRL_SQUARE) {
            premult = !premult;
            if (premult)
                gxmClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            else
                gxmClearColor(0.3f, 0.3f, 0.32f, 1.0f);
        }

        // mouse cursor emulation
        mx += pad.buttons & SCE_CTRL_RIGHT ? MOUSE_MOVE : 0;
        mx -= pad.buttons & SCE_CTRL_LEFT ? MOUSE_MOVE : 0;
        my += pad.buttons & SCE_CTRL_DOWN ? MOUSE_MOVE : 0;
        my -= pad.buttons & SCE_CTRL_UP ? MOUSE_MOVE : 0;
        CLAMP_NUM(mx, 0, DISPLAY_WIDTH)
        CLAMP_NUM(my, 0, DISPLAY_HEIGHT)

        t = (float) sceKernelGetProcessTimeLow() / 1000000.0f;
        dt = t - prevt;
        prevt = t;

        gxmBeginFrame();
        gxmClear();

        nvgBeginFrame(vg, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1.0f);
        renderDemo(vg, (float) mx, (float) my, DISPLAY_WIDTH, DISPLAY_HEIGHT, (float) t, blowup, &data);
        renderImage(vg, compressedImage, 415, 5, 133, 100);
        renderGraph(vg, 5, 5, &fps);
        renderGraph(vg, 5 + 200 + 5, 5, &cpuGraph);
        nvgEndFrame(vg);

        gxmEndFrame();
        gxmSwapBuffer();

        cpuTime = (float) sceKernelGetProcessTimeLow() / 1000000.0f - t;
        updateGraph(&fps, (float) dt);
        updateGraph(&cpuGraph, (float) cpuTime);

        if (screenshot) {
            screenshot = 0;
            saveScreenShot(DISPLAY_WIDTH, DISPLAY_HEIGHT, premult, "ux0:data/nanovg-gxm-screenshot.png");
        }

    }

    freeDemoData(vg, &data);
    nvgDeleteGXM(vg);
    gxmDeleteWindow(window);

    sceClibPrintf("Average Frame Time: %.2f ms\n", getGraphAverage(&fps) * 1000.0f);
    sceClibPrintf("          CPU Time: %.2f ms\n", getGraphAverage(&cpuGraph) * 1000.0f);

    return EXIT_SUCCESS;
}
