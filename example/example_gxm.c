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

#undef sceClibPrintf

#define MOUSE_MOVE 5
#define CLAMP_NUM(a, min, max) { if(a>max) a=max; if(a<min) a=min; }

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

    NVGXMframebuffer *gxm = NULL;
    NVGXMinitOptions initOptions = {
            .msaa = SCE_GXM_MULTISAMPLE_4X,
            .swapInterval = 1,
            .dumpShader = 1, // Save shaders to ux0:data/nvg_*.c
            .scenesPerFrame = 1,
    };

    gxm = nvgxmCreateWindow(&initOptions);
    if (gxm == NULL) {
        sceClibPrintf("gxm: failed to initialize\n");
        return EXIT_FAILURE;
    }

    vg = nvgCreateGXM(gxm, NVG_STENCIL_STROKES);
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
    nvgxmDeleteWindow(gxm);

    sceClibPrintf("Average Frame Time: %.2f ms\n", getGraphAverage(&fps) * 1000.0f);
    sceClibPrintf("          CPU Time: %.2f ms\n", getGraphAverage(&cpuGraph) * 1000.0f);

    return EXIT_SUCCESS;
}
