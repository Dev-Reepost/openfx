// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#include "comfyui_image_io.h"
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

namespace ComfyUI {
namespace ImageIO {

// Helper function to create directory recursively
bool createDirectoryRecursive(const std::string& path) {
    if (path.empty()) return true;

    // Check if directory already exists
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true; // Already exists
    }

    // Find parent directory
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string parent = path.substr(0, pos);
        if (!createDirectoryRecursive(parent)) {
            return false;
        }
    }

    // Create this directory
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}

// Helper function to get directory from file path
static std::string getDirectory(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos != std::string::npos) {
        return filepath.substr(0, pos);
    }
    return "";
}

ImageData readEXR(const std::string& filename) {
    float* rgba = nullptr;
    int width, height;
    const char* err = nullptr;

    // Load EXR image
    int ret = LoadEXR(&rgba, &width, &height, filename.c_str(), &err);

    if (ret != TINYEXR_SUCCESS) {
        std::string error_msg = "Failed to load EXR file: " + filename;
        if (err) {
            error_msg += " - " + std::string(err);
            FreeEXRErrorMessage(err);
        }
        throw std::runtime_error(error_msg);
    }

    // Create ImageData
    ImageData image(width, height, 4); // TinyEXR always loads as RGBA

    // Copy pixel data
    std::memcpy(image.pixels.data(), rgba, width * height * 4 * sizeof(float));

    // Free TinyEXR memory
    free(rgba);

    return image;
}

void writeEXR(const std::string& filename, const ImageData& image) {
    if (image.width <= 0 || image.height <= 0) {
        throw std::runtime_error("Invalid image dimensions");
    }

    if (image.channels != 3 && image.channels != 4) {
        throw std::runtime_error("Only RGB and RGBA images supported");
    }

    // Create parent directory if it doesn't exist
    std::string dir = getDirectory(filename);
    if (!dir.empty() && !createDirectoryRecursive(dir)) {
        throw std::runtime_error("Failed to create directory: " + dir);
    }

    // TinyEXR expects non-interleaved channel data
    std::vector<float> r(image.width * image.height);
    std::vector<float> g(image.width * image.height);
    std::vector<float> b(image.width * image.height);
    std::vector<float> a(image.width * image.height);

    // De-interleave channels
    for (int i = 0; i < image.width * image.height; ++i) {
        r[i] = image.pixels[i * image.channels + 0];
        g[i] = image.pixels[i * image.channels + 1];
        b[i] = image.pixels[i * image.channels + 2];
        if (image.channels == 4) {
            a[i] = image.pixels[i * image.channels + 3];
        } else {
            a[i] = 1.0f; // Opaque
        }
    }

    // Setup EXR image structure
    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage exr_image;
    InitEXRImage(&exr_image);

    exr_image.num_channels = 4;

    std::vector<float*> image_ptr(4);
    image_ptr[0] = &b[0]; // B
    image_ptr[1] = &g[0]; // G
    image_ptr[2] = &r[0]; // R
    image_ptr[3] = &a[0]; // A

    exr_image.images = (unsigned char**)image_ptr.data();
    exr_image.width = image.width;
    exr_image.height = image.height;

    header.num_channels = 4;
    header.channels = (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * 4);

    // Must be in BGRA order for EXR
    strncpy(header.channels[0].name, "B", 255);
    strncpy(header.channels[1].name, "G", 255);
    strncpy(header.channels[2].name, "R", 255);
    strncpy(header.channels[3].name, "A", 255);

    header.pixel_types = (int*)malloc(sizeof(int) * 4);
    header.requested_pixel_types = (int*)malloc(sizeof(int) * 4);
    for (int i = 0; i < 4; i++) {
        header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF; // Save as 16-bit float
    }

    // Write file
    const char* err = nullptr;
    int ret = SaveEXRImageToFile(&exr_image, &header, filename.c_str(), &err);

    // Cleanup
    free(header.channels);
    free(header.pixel_types);
    free(header.requested_pixel_types);

    if (ret != TINYEXR_SUCCESS) {
        std::string error_msg = "Failed to write EXR file: " + filename;
        if (err) {
            error_msg += " - " + std::string(err);
            FreeEXRErrorMessage(err);
        }
        throw std::runtime_error(error_msg);
    }
}

ImageData fromOFXBuffer(
    const void* srcPixels,
    int width,
    int height,
    int rowBytes,
    int pixelComponents,
    int bitDepth
) {
    if (!srcPixels) {
        throw std::runtime_error("Source pixel buffer is null!");
    }

    ImageData image(width, height, pixelComponents);

    const uint8_t* src8 = static_cast<const uint8_t*>(srcPixels);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src8 + y * rowBytes;

        for (int x = 0; x < width; ++x) {
            int dstIdx = (y * width + x) * pixelComponents;

            if (bitDepth == 8) {
                // 8-bit unsigned
                const uint8_t* pixel = srcRow + x * pixelComponents;
                for (int c = 0; c < pixelComponents; ++c) {
                    image.pixels[dstIdx + c] = pixel[c] / 255.0f;
                }
            }
            else if (bitDepth == 16) {
                // 16-bit unsigned
                const uint16_t* pixel = reinterpret_cast<const uint16_t*>(srcRow + x * pixelComponents * 2);
                for (int c = 0; c < pixelComponents; ++c) {
                    image.pixels[dstIdx + c] = pixel[c] / 65535.0f;
                }
            }
            else if (bitDepth == 32) {
                // 32-bit float
                const float* pixel = reinterpret_cast<const float*>(srcRow + x * pixelComponents * 4);
                for (int c = 0; c < pixelComponents; ++c) {
                    image.pixels[dstIdx + c] = pixel[c];
                }
            }
            else {
                throw std::runtime_error("Unsupported bit depth: " + std::to_string(bitDepth));
            }
        }
    }

    return image;
}

void toOFXBuffer(
    const ImageData& image,
    void* dstPixels,
    int rowBytes,
    int pixelComponents,
    int bitDepth
) {
    // Defensive parameter validation (prevents crashes from invalid input)
    if (!dstPixels) {
        throw std::invalid_argument("toOFXBuffer: dstPixels is NULL - cannot write to invalid buffer");
    }

    if (image.pixels.empty()) {
        throw std::invalid_argument("toOFXBuffer: source image pixel data is empty");
    }

    if (image.width <= 0 || image.height <= 0) {
        throw std::invalid_argument("toOFXBuffer: invalid image dimensions: " +
                                   std::to_string(image.width) + "x" + std::to_string(image.height));
    }

    if (pixelComponents < 1 || pixelComponents > 4) {
        throw std::invalid_argument("toOFXBuffer: invalid pixel components: " +
                                   std::to_string(pixelComponents));
    }

    if (bitDepth != 8 && bitDepth != 16 && bitDepth != 32) {
        throw std::invalid_argument("toOFXBuffer: unsupported bit depth: " +
                                   std::to_string(bitDepth));
    }

    int bytesPerPixel = pixelComponents * (bitDepth / 8);
    int minRowBytes = image.width * bytesPerPixel;

    if (rowBytes < minRowBytes) {
        throw std::invalid_argument("toOFXBuffer: rowBytes (" + std::to_string(rowBytes) +
                                   ") is smaller than minimum required (" + std::to_string(minRowBytes) + ")");
    }

    size_t expectedPixelCount = static_cast<size_t>(image.width) * image.height * image.channels;
    if (image.pixels.size() < expectedPixelCount) {
        throw std::invalid_argument("toOFXBuffer: source image pixel array is too small - expected " +
                                   std::to_string(expectedPixelCount) + " floats, got " +
                                   std::to_string(image.pixels.size()));
    }

    uint8_t* dst8 = static_cast<uint8_t*>(dstPixels);

    for (int y = 0; y < image.height; ++y) {
        uint8_t* dstRow = dst8 + y * rowBytes;

        for (int x = 0; x < image.width; ++x) {
            int srcIdx = (y * image.width + x) * image.channels;

            if (bitDepth == 8) {
                // 8-bit unsigned
                uint8_t* pixel = dstRow + x * pixelComponents;
                for (int c = 0; c < std::min(pixelComponents, image.channels); ++c) {
                    float val = std::max(0.0f, std::min(1.0f, image.pixels[srcIdx + c]));
                    pixel[c] = static_cast<uint8_t>(val * 255.0f + 0.5f);
                }
            }
            else if (bitDepth == 16) {
                // 16-bit unsigned
                uint16_t* pixel = reinterpret_cast<uint16_t*>(dstRow + x * pixelComponents * 2);
                for (int c = 0; c < std::min(pixelComponents, image.channels); ++c) {
                    float val = std::max(0.0f, std::min(1.0f, image.pixels[srcIdx + c]));
                    pixel[c] = static_cast<uint16_t>(val * 65535.0f + 0.5f);
                }
            }
            else if (bitDepth == 32) {
                // 32-bit float
                float* pixel = reinterpret_cast<float*>(dstRow + x * pixelComponents * 4);
                for (int c = 0; c < std::min(pixelComponents, image.channels); ++c) {
                    pixel[c] = image.pixels[srcIdx + c];
                }
            }
            else {
                throw std::runtime_error("Unsupported bit depth: " + std::to_string(bitDepth));
            }

            // Fill remaining channels with default values (e.g., alpha = 1.0)
            if (pixelComponents > image.channels) {
                if (bitDepth == 8) {
                    uint8_t* pixel = dstRow + x * pixelComponents;
                    for (int c = image.channels; c < pixelComponents; ++c) {
                        pixel[c] = (c == 3) ? 255 : 0; // Alpha = 1.0, others = 0.0
                    }
                }
                else if (bitDepth == 16) {
                    uint16_t* pixel = reinterpret_cast<uint16_t*>(dstRow + x * pixelComponents * 2);
                    for (int c = image.channels; c < pixelComponents; ++c) {
                        pixel[c] = (c == 3) ? 65535 : 0;
                    }
                }
                else if (bitDepth == 32) {
                    float* pixel = reinterpret_cast<float*>(dstRow + x * pixelComponents * 4);
                    for (int c = image.channels; c < pixelComponents; ++c) {
                        pixel[c] = (c == 3) ? 1.0f : 0.0f;
                    }
                }
            }
        }
    }
}

} // namespace ImageIO
} // namespace ComfyUI
