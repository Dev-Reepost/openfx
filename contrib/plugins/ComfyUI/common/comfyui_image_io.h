// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef COMFYUI_IMAGE_IO_H
#define COMFYUI_IMAGE_IO_H

#include <string>
#include <vector>
#include <stdexcept>

namespace ComfyUI {

/**
 * @brief Simple image data structure for EXR files
 */
struct ImageData {
    int width;
    int height;
    int channels;  // 3 for RGB, 4 for RGBA
    std::vector<float> pixels;  // Interleaved float data (RGBRGBRGB... or RGBARGBARGBA...)

    ImageData() : width(0), height(0), channels(0) {}
    ImageData(int w, int h, int c) : width(w), height(h), channels(c) {
        pixels.resize(w * h * c);
    }
};

/**
 * @brief Image I/O utilities for ComfyUI integration
 *
 * Handles reading and writing EXR files using TinyEXR.
 * Provides conversion between OFX image buffers and EXR files.
 */
namespace ImageIO {

/**
 * @brief Create directory recursively
 *
 * Creates all parent directories as needed (like mkdir -p).
 *
 * @param path Directory path to create
 * @return true if directory exists or was created successfully, false otherwise
 */
bool createDirectoryRecursive(const std::string& path);

/**
 * @brief Read an EXR file
 *
 * @param filename Path to EXR file
 * @return ImageData structure with pixel data
 * @throws std::runtime_error if file cannot be read
 */
ImageData readEXR(const std::string& filename);

/**
 * @brief Write an EXR file
 *
 * @param filename Path to output EXR file
 * @param image Image data to write
 * @throws std::runtime_error if file cannot be written
 */
void writeEXR(const std::string& filename, const ImageData& image);

/**
 * @brief Convert OFX image buffer to ImageData
 *
 * Handles various OFX pixel formats and converts to float RGB/RGBA.
 *
 * @param srcPixels Source pixel buffer
 * @param width Image width
 * @param height Image height
 * @param rowBytes Bytes per row (may include padding)
 * @param pixelComponents Number of components (3=RGB, 4=RGBA)
 * @param bitDepth Bit depth per component (8, 16, or 32)
 * @return ImageData with float pixels
 */
ImageData fromOFXBuffer(
    const void* srcPixels,
    int width,
    int height,
    int rowBytes,
    int pixelComponents,
    int bitDepth
);

/**
 * @brief Convert ImageData to OFX image buffer
 *
 * @param image Source image data
 * @param dstPixels Destination pixel buffer
 * @param rowBytes Bytes per row (may include padding)
 * @param pixelComponents Number of components in destination
 * @param bitDepth Bit depth per component (8, 16, or 32)
 */
void toOFXBuffer(
    const ImageData& image,
    void* dstPixels,
    int rowBytes,
    int pixelComponents,
    int bitDepth
);

} // namespace ImageIO
} // namespace ComfyUI

#endif // COMFYUI_IMAGE_IO_H
