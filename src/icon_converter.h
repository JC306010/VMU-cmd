#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ICONDATA_VMS icon dimensions
static constexpr int VMU_ICON_W = 32;
static constexpr int VMU_ICON_H = 32;

// Max size for ICONDATA_VMS (2 blocks = 1024 bytes typical)
static constexpr size_t ICONDATA_VMS_MAX_SIZE = 1024;
// Offset for color icon within ICONDATA_VMS
static constexpr int ICONDATA_COLOR_OFFSET = 24 + 128; // after header + mono

// Create ICONDATA_VMS binary from a PNG (generates both mono + 16-color icons)
// PNG is scaled to 32x32
bool create_icondata_vms (const std::string &png_path,
                          std::vector<uint8_t> &out_data);

// Decode ICONDATA_VMS mono icon (32x32 grayscale, 0 or 255)
bool decode_icondata_vms (const std::vector<uint8_t> &data,
                          std::vector<uint8_t> &out_mono_32x32);

// Decode ICONDATA_VMS full (mono + 16-color + description)
bool decode_icondata_vms_full (const std::vector<uint8_t> &data,
                               std::vector<uint8_t> &out_mono_32x32,
                               std::vector<uint8_t> &out_color_32x32_rgba,
                               std::string &out_description);

// Write a 32x32 grayscale image to PNG (scaled up)
bool write_grayscale_png (const std::vector<uint8_t> &pixels_32x32,
                          const std::string &png_path, int scale = 4);

// Write a 32x32 RGBA image to PNG (scaled up)
bool write_rgba_png (const std::vector<uint8_t> &rgba_32x32,
                     const std::string &png_path, int scale = 4);
