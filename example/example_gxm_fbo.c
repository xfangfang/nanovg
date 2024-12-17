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

#include "perf.h"

#define NVG_IMAGE 1

int image = 0;

void renderPattern(NVGcontext *vg, NVGXMframebuffer *fb, float t) {
    int winWidth, winHeight;
    int pw, ph, x, y;
    float s = 20.0f;
    float sr = (cosf(t) + 1) * 0.5f;
    float r = s * 0.6f * (0.2f + 0.8f * sr);

    if (fb == NULL) return;

    nvgImageSize(vg, image, &winWidth, &winHeight);

    // Draw some stuff to an FBO as a test
    gxmBeginFrameEx(fb, 0);
    gxmClearColor(0, 0, 0, 0);
    gxmClear();
    nvgBeginFrame(vg, winWidth, winHeight, 1.0f);

    pw = (int) ceilf(winWidth / s);
    ph = (int) ceilf(winHeight / s);

    nvgBeginPath(vg);
    for (y = 0; y < ph; y++) {
        for (x = 0; x < pw; x++) {
            float cx = (x + 0.5f) * s;
            float cy = (y + 0.5f) * s;
            nvgCircle(vg, cx, cy, r);
        }
    }
    nvgFillColor(vg, nvgRGBA(220, 160, 0, 200));
    nvgFill(vg);

    nvgEndFrame(vg);
    gxmEndFrame();
}

int loadFonts(NVGcontext *vg) {
    int font;
    font = nvgCreateFont(vg, "sans", "../example/Roboto-Regular.ttf");
    if (font == -1) {
        printf("Could not add font regular.\n");
        return -1;
    }
    font = nvgCreateFont(vg, "sans-bold", "../example/Roboto-Bold.ttf");
    if (font == -1) {
        printf("Could not add font bold.\n");
        return -1;
    }
    return 0;
}

int main() {
    PerfGraph fps, cpuGraph;
    SceCtrlData pad;
    NVGcontext *vg = NULL;
    double prevt = 0, cpuTime = 0;
    double t, dt;
    int i;
    int texture_width = 100, texture_height = 100;
    int texture_stride = ALIGN(texture_width, 8);

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

    initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");
    initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");

#ifdef NVG_IMAGE
    image = nvgCreateImageRGBA(vg, texture_width, texture_height, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY, NULL);
    if (image == 0) {
        sceClibPrintf("nanovg: failed to create image\n");
        return EXIT_FAILURE;
    }

    NVGXMtexture *texture = nvgxmImageHandle(vg, image);
#else
    NVGXMtexture *texture = gxmCreateTexture(texture_width, texture_height, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, NULL);
    if (texture == NULL) {
        sceClibPrintf("gxm: failed to allocate texture\n");
        return EXIT_FAILURE;
    }
    sceGxmTextureSetUAddrMode(&texture->tex, SCE_GXM_TEXTURE_ADDR_REPEAT);
    sceGxmTextureSetVAddrMode(&texture->tex, SCE_GXM_TEXTURE_ADDR_REPEAT);

    image = nvgxmCreateImageFromHandle(vg, &texture->tex);
#endif

    NVGXMframebuffer *fb = NULL;
    NVGXMframebufferInitOptions framebufferOpts = {
            .display_buffer_count = 1, // Must be 1 for custom FBOs
            .scenesPerFrame = 1,
            .render_target = texture,
            .color_format = SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
            .color_surface_type = SCE_GXM_COLOR_SURFACE_LINEAR,
            .display_width = texture_width,
            .display_height = texture_height,
            .display_stride = texture_stride,
    };
    fb = gxmCreateFramebuffer(&framebufferOpts);
    if (!fb) {
        sceClibPrintf("gxm: failed to create framebuffer\n");
        return EXIT_FAILURE;
    }

    if (loadFonts(vg) == -1) {
        sceClibPrintf("Could not load fonts\n");
        return EXIT_FAILURE;
    }

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START)
            break;

        t = (float) sceKernelGetProcessTimeLow() / 1000000.0f;
        dt = t - prevt;
        prevt = t;

        renderPattern(vg, fb, t);

        gxmBeginFrame();
        gxmClearColor(0.3f, 0.3f, 0.32f, 1.0f);
        gxmClear();

        nvgBeginFrame(vg, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1.0f);

        // Use the FBO as image pattern.
        {
            NVGpaint img = nvgImagePattern(vg, 0, 0, texture_width, texture_stride, 0, image, 1.0f);
            nvgSave(vg);

            for (i = 0; i < 20; i++) {
                nvgBeginPath(vg);
                nvgRect(vg, 10 + i*30,10, 10, DISPLAY_HEIGHT-20);
                nvgFillColor(vg, nvgHSLA(i/19.0f, 0.5f, 0.5f, 255));
                nvgFill(vg);
            }

            nvgBeginPath(vg);
            nvgRoundedRect(vg, 140 + sinf(t*1.3f)*100, 140 + cosf(t*1.71244f)*100, 250, 250, 20);
            nvgFillPaint(vg, img);
            nvgFill(vg);
            nvgStrokeColor(vg, nvgRGBA(220,160,0,255));
            nvgStrokeWidth(vg, 3.0f);
            nvgStroke(vg);

            nvgRestore(vg);
        }

        renderGraph(vg, 5, 5, &fps);
        renderGraph(vg, 5 + 200 + 5, 5, &cpuGraph);
        nvgEndFrame(vg);

        gxmEndFrame();
        gxmSwapBuffer();

        cpuTime = (float) sceKernelGetProcessTimeLow() / 1000000.0f - t;
        updateGraph(&fps, (float) dt);
        updateGraph(&cpuGraph, (float) cpuTime);

    }

    nvgDeleteGXM(vg);
#ifndef NVG_IMAGE
    gxmDeleteTexture(texture);
#endif
    gxmDeleteFramebuffer(fb);
    gxmDeleteWindow(window);

    sceClibPrintf("Average Frame Time: %.2f ms\n", getGraphAverage(&fps) * 1000.0f);
    sceClibPrintf("          CPU Time: %.2f ms\n", getGraphAverage(&cpuGraph) * 1000.0f);

    return EXIT_SUCCESS;
}
