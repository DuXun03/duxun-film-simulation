/**
 * DuXunProcess.h
 * ==============
 * Image processing core for 独寻胶片模拟 OFX plugin.
 */

#ifndef DUXUN_PROCESS_H
#define DUXUN_PROCESS_H

#include "DuXunPlugin.h"

/**
 * Process a frame of image data.
 *
 * src/dst: float32 RGBA interleaved pixel data
 * width/height: render region dimensions
 * srcRowBytes/dstRowBytes: bytes per row (may include padding)
 * data: current parameter state
 */
void processImage(
    const float* src,
    float* dst,
    int width, int height,
    int srcRowBytes, int dstRowBytes,
    const DuXunInstanceData* data
);

#endif
