#include "icon_converter.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <png.h>
#include <unordered_map>
#include <vector>

static bool
read_png (const std::string &path, std::vector<uint8_t> &out_pixels, int &out_w,
          int &out_h)
{
  FILE *fp = std::fopen (path.c_str (), "rb");
  if (!fp)
    return false;

  png_structp png = png_create_read_struct (PNG_LIBPNG_VER_STRING, nullptr,
                                            nullptr, nullptr);
  if (!png)
    {
      std::fclose (fp);
      return false;
    }

  png_infop info = png_create_info_struct (png);
  if (!info)
    {
      png_destroy_read_struct (&png, nullptr, nullptr);
      std::fclose (fp);
      return false;
    }

  if (setjmp (png_jmpbuf (png)))
    {
      png_destroy_read_struct (&png, &info, nullptr);
      std::fclose (fp);
      return false;
    }

  png_init_io (png, fp);
  png_read_info (png, info);

  out_w = png_get_image_width (png, info);
  out_h = png_get_image_height (png, info);
  png_byte color_type = png_get_color_type (png, info);
  png_byte bit_depth = png_get_bit_depth (png, info);

  if (bit_depth == 16)
    png_set_strip_16 (png);
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb (png);
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8 (png);
  if (png_get_valid (png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha (png);
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY
      || color_type == PNG_COLOR_TYPE_PALETTE)
    {
      png_set_filler (png, 0xFF, PNG_FILLER_AFTER);
    }
  if (color_type == PNG_COLOR_TYPE_GRAY
      || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
      png_set_gray_to_rgb (png);
    }

  png_read_update_info (png, info);

  std::vector<png_bytep> rows (out_h);
  std::vector<uint8_t> raw_data (out_h * out_w * 4);
  for (int y = 0; y < out_h; y++)
    rows[y] = &raw_data[y * out_w * 4];

  png_read_image (png, rows.data ());
  png_destroy_read_struct (&png, &info, nullptr);
  std::fclose (fp);

  out_pixels = std::move (raw_data);
  return true;
}

static bool
write_png (const std::string &path, const std::vector<uint8_t> &pixels, int w,
           int h, int channels)
{
  FILE *fp = std::fopen (path.c_str (), "wb");
  if (!fp)
    return false;

  png_structp png = png_create_write_struct (PNG_LIBPNG_VER_STRING, nullptr,
                                             nullptr, nullptr);
  if (!png)
    {
      std::fclose (fp);
      return false;
    }

  png_infop info = png_create_info_struct (png);
  if (!info)
    {
      png_destroy_write_struct (&png, nullptr);
      std::fclose (fp);
      return false;
    }

  if (setjmp (png_jmpbuf (png)))
    {
      png_destroy_write_struct (&png, &info);
      std::fclose (fp);
      return false;
    }

  png_init_io (png, fp);

  int color_type;
  if (channels == 4)
    color_type = PNG_COLOR_TYPE_RGBA;
  else if (channels == 3)
    color_type = PNG_COLOR_TYPE_RGB;
  else
    color_type = PNG_COLOR_TYPE_GRAY;

  png_set_IHDR (png, info, w, h, 8, color_type, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info (png, info);

  std::vector<png_bytep> rows (h);
  for (int y = 0; y < h; y++)
    rows[y] = const_cast<png_bytep> (&pixels[y * w * channels]);

  png_write_image (png, rows.data ());
  png_write_end (png, nullptr);
  png_destroy_write_struct (&png, &info);
  std::fclose (fp);
  return true;
}

// Scale RGBA image to target dimensions (nearest-neighbor)
static void
scale_rgba (const std::vector<uint8_t> &src, int src_w, int src_h,
            std::vector<uint8_t> &dst, int dst_w, int dst_h)
{
  dst.resize (dst_w * dst_h * 4);
  for (int y = 0; y < dst_h; y++)
    {
      for (int x = 0; x < dst_w; x++)
        {
          int sx = std::min ((x * src_w) / dst_w, src_w - 1);
          int sy = std::min ((y * src_h) / dst_h, src_h - 1);
          int si = (sy * src_w + sx) * 4;
          int di = (y * dst_w + x) * 4;
          for (int c = 0; c < 4; c++)
            dst[di + c] = src[si + c];
        }
    }
}

// ---- ICONDATA_VMS encoding ----

// Encode as mono: 32x32 1bpp, row-major, 4 bytes/row, MSB=leftmost
static void
encode_mono_bitmap (const std::vector<uint8_t> &rgba_32x32, uint8_t out[128])
{
  std::memset (out, 0, 128);
  for (int y = 0; y < 32; y++)
    {
      for (int x = 0; x < 32; x++)
        {
          int si = (y * 32 + x) * 4;
          uint8_t r = rgba_32x32[si];
          uint8_t g = rgba_32x32[si + 1];
          uint8_t b = rgba_32x32[si + 2];
          uint8_t a = rgba_32x32[si + 3];
          int lum = (r * 77 + g * 150 + b * 29) >> 8;
          bool dark = (lum < 128 && a > 128);
          int byte_idx = y * 4 + (x / 8);
          int bit_idx = 7 - (x % 8);
          if (dark)
            out[byte_idx] |= (1 << bit_idx);
        }
    }
}

// Quantize RGBA to ARGB4444 (packed as uint16 LE)
static uint16_t
rgba_to_argb4444 (uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
  uint16_t a4 = (a >> 4) & 0xF;
  uint16_t r4 = (r >> 4) & 0xF;
  uint16_t g4 = (g >> 4) & 0xF;
  uint16_t b4 = (b >> 4) & 0xF;
  return (a4 << 12) | (r4 << 8) | (g4 << 4) | b4;
}

// Expand ARGB4444 back to RGBA bytes
static void
argb4444_to_rgba (uint16_t argb, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a)
{
  a = ((argb >> 12) & 0xF) * 17;
  r = ((argb >> 8) & 0xF) * 17;
  g = ((argb >> 4) & 0xF) * 17;
  b = (argb & 0xF) * 17;
}

// Encode color icon: palette (32 bytes) + nibble bitmap (512 bytes)
static void
encode_color_icon (const std::vector<uint8_t> &rgba_32x32,
                   uint8_t out_palette[32], uint8_t out_bitmap[512])
{
  // Step 1: quantize each pixel to ARGB4444 and collect unique colors
  struct ColorKey
  {
    uint16_t argb;
    bool
    operator== (const ColorKey &o) const
    {
      return argb == o.argb;
    }
  };
  struct ColorHash
  {
    size_t
    operator() (const ColorKey &k) const
    {
      return k.argb;
    }
  };

  std::unordered_map<ColorKey, int, ColorHash> color_counts;
  std::vector<uint16_t> pixel_argb (32 * 32);

  for (int i = 0; i < 32 * 32; i++)
    {
      int si = i * 4;
      uint16_t argb = rgba_to_argb4444 (rgba_32x32[si], rgba_32x32[si + 1],
                                        rgba_32x32[si + 2], rgba_32x32[si + 3]);
      pixel_argb[i] = argb;
      color_counts[{ argb }]++;
    }

  // Build initial palette from unique colors
  std::vector<uint16_t> palette;
  for (auto &kv : color_counts)
    {
      palette.push_back (kv.first.argb);
    }

  // If > 16 colors, reduce by merging closest pairs
  auto color_distance = [] (uint16_t a, uint16_t b) -> float {
    uint8_t ar, ag, ab, aa;
    uint8_t br, bg, bb, ba;
    argb4444_to_rgba (a, ar, ag, ab, aa);
    argb4444_to_rgba (b, br, bg, bb, ba);
    float dr = (int)ar - (int)br;
    float dg = (int)ag - (int)bg;
    float db = (int)ab - (int)bb;
    float da = (int)aa - (int)ba;
    // Weight alpha less
    return dr * dr + dg * dg + db * db + da * da * 0.25f;
  };

  while (palette.size () > 16)
    {
      int best_i = 0, best_j = 1;
      float best_dist = std::numeric_limits<float>::max ();
      for (size_t i = 0; i < palette.size (); i++)
        {
          for (size_t j = i + 1; j < palette.size (); j++)
            {
              float d = color_distance (palette[i], palette[j]);
              if (d < best_dist)
                {
                  best_dist = d;
                  best_i = i;
                  best_j = j;
                }
            }
        }
      // Average the two closest colors
      uint8_t r1, g1, b1, a1, r2, g2, b2, a2;
      argb4444_to_rgba (palette[best_i], r1, g1, b1, a1);
      argb4444_to_rgba (palette[best_j], r2, g2, b2, a2);
      uint16_t merged = rgba_to_argb4444 ((r1 + r2) / 2, (g1 + g2) / 2,
                                          (b1 + b2) / 2, (a1 + a2) / 2);
      palette[best_i] = merged;
      palette.erase (palette.begin () + best_j);
    }

  // Pad palette to 16 entries
  while (palette.size () < 16)
    {
      palette.push_back (0x0FFF); // transparent black
    }

  // Write palette (16 LE uint16 ARGB4444)
  for (int i = 0; i < 16; i++)
    {
      out_palette[i * 2] = palette[i] & 0xFF;
      out_palette[i * 2 + 1] = (palette[i] >> 8) & 0xFF;
    }

  // Map each pixel to nearest palette index
  auto nearest_index = [&] (uint16_t argb) -> int {
    int best_idx = 0;
    float best_dist = std::numeric_limits<float>::max ();
    for (int i = 0; i < 16; i++)
      {
        float d = color_distance (argb, palette[i]);
        if (d < best_dist)
          {
            best_dist = d;
            best_idx = i;
          }
      }
    return best_idx;
  };

  // Write nibble bitmap (high nibble = left pixel)
  std::memset (out_bitmap, 0, 512);
  for (int y = 0; y < 32; y++)
    {
      for (int x = 0; x < 32; x++)
        {
          int pi = y * 32 + x;
          int idx = nearest_index (pixel_argb[pi]);
          int byte_off = y * 16 + (x / 2);
          if (x % 2 == 0)
            {
              out_bitmap[byte_off] |= (idx << 4); // high nibble = left
            }
          else
            {
              out_bitmap[byte_off] |= idx; // low nibble = right
            }
        }
    }
}

bool
create_icondata_vms (const std::string &png_path,
                     std::vector<uint8_t> &out_data)
{
  std::vector<uint8_t> rgba;
  int w, h;
  if (!read_png (png_path, rgba, w, h))
    return false;

  // Scale to 32x32
  std::vector<uint8_t> rgba_32;
  scale_rgba (rgba, w, h, rgba_32, 32, 32);

  // Build ICONDATA_VMS structure:
  // [0..15]  : description (16 bytes)
  // [16..19] : mono offset (LE uint32)
  // [20..23] : color offset (LE uint32)
  // [24..151]: mono bitmap (128 bytes at offset 24)
  // [152..183]: palette (32 bytes at offset 152 = 24+128)
  // [184..695]: color bitmap (512 bytes at offset 184 = 24+128+32)
  // Total: 696 bytes, pad to 1024

  out_data.resize (ICONDATA_VMS_MAX_SIZE, 0);

  // Description
  const char *desc = "png2vmu icon";
  std::memcpy (out_data.data (), desc, std::min (strlen (desc), size_t (15)));

  // Offsets
  uint32_t mono_off = 24;
  uint32_t color_off = ICONDATA_COLOR_OFFSET; // 24 + 128 = 152
  out_data[16] = mono_off & 0xFF;
  out_data[17] = (mono_off >> 8) & 0xFF;
  out_data[18] = (mono_off >> 16) & 0xFF;
  out_data[19] = (mono_off >> 24) & 0xFF;
  out_data[20] = color_off & 0xFF;
  out_data[21] = (color_off >> 8) & 0xFF;
  out_data[22] = (color_off >> 16) & 0xFF;
  out_data[23] = (color_off >> 24) & 0xFF;

  // Encode mono bitmap at offset 24
  encode_mono_bitmap (rgba_32, &out_data[24]);

  // Encode color icon at offset 152
  encode_color_icon (rgba_32, &out_data[color_off], &out_data[color_off + 32]);

  return true;
}

// ---- ICONDATA_VMS decoding ----

bool
decode_icondata_vms (const std::vector<uint8_t> &data,
                     std::vector<uint8_t> &out_mono_32x32)
{
  if (data.size () < 24)
    return false;

  uint32_t mono_off
      = data[16] | (data[17] << 8) | (data[18] << 16) | (data[19] << 24);
  if (mono_off + 128 > data.size ())
    return false;

  out_mono_32x32.resize (32 * 32);
  for (int y = 0; y < 32; y++)
    {
      for (int x = 0; x < 32; x++)
        {
          int byte_idx = mono_off + y * 4 + (x / 8);
          int bit_idx = 7 - (x % 8);
          uint8_t val = (data[byte_idx] >> bit_idx) & 1;
          out_mono_32x32[y * 32 + x] = val ? 0 : 255;
        }
    }
  return true;
}

bool
decode_icondata_vms_full (const std::vector<uint8_t> &data,
                          std::vector<uint8_t> &out_mono_32x32,
                          std::vector<uint8_t> &out_color_32x32_rgba,
                          std::string &out_description)
{
  if (data.size () < 24)
    return false;

  // Description
  out_description.assign (reinterpret_cast<const char *> (data.data ()), 16);
  // Trim nulls/spaces
  while (!out_description.empty ()
         && (out_description.back () == '\0' || out_description.back () == ' '))
    out_description.pop_back ();

  // Mono
  if (!decode_icondata_vms (data, out_mono_32x32))
    return false;

  // Color
  uint32_t color_off
      = data[20] | (data[21] << 8) | (data[22] << 16) | (data[23] << 24);
  if (color_off == 0 || color_off + 32 + 512 > data.size ())
    {
      out_color_32x32_rgba.clear ();
      return true; // no color icon, not an error
    }

  // Read palette
  uint16_t palette[16];
  for (int i = 0; i < 16; i++)
    {
      palette[i] = data[color_off + i * 2] | (data[color_off + i * 2 + 1] << 8);
    }

  // Decode nibble bitmap
  out_color_32x32_rgba.resize (32 * 32 * 4);
  for (int y = 0; y < 32; y++)
    {
      for (int x = 0; x < 32; x++)
        {
          int byte_off = color_off + 32 + y * 16 + (x / 2);
          int nibble;
          if (x % 2 == 0)
            nibble = (data[byte_off] >> 4) & 0xF;
          else
            nibble = data[byte_off] & 0xF;
          int idx = y * 32 + x;
          uint8_t r, g, b, a;
          argb4444_to_rgba (palette[nibble], r, g, b, a);
          out_color_32x32_rgba[idx * 4] = r;
          out_color_32x32_rgba[idx * 4 + 1] = g;
          out_color_32x32_rgba[idx * 4 + 2] = b;
          out_color_32x32_rgba[idx * 4 + 3] = a;
        }
    }

  return true;
}

// ---- PNG output helpers ----

bool
write_grayscale_png (const std::vector<uint8_t> &pixels_32x32,
                     const std::string &png_path, int scale)
{
  if (pixels_32x32.size () != 32 * 32)
    return false;
  int ow = 32 * scale;
  int oh = 32 * scale;
  std::vector<uint8_t> rgba (ow * oh * 4);
  for (int y = 0; y < oh; y++)
    {
      for (int x = 0; x < ow; x++)
        {
          uint8_t p = pixels_32x32[(y / scale) * 32 + (x / scale)];
          int di = (y * ow + x) * 4;
          if (p > 128)
            {
              rgba[di] = 0xFF;
              rgba[di + 1] = 0xFF;
              rgba[di + 2] = 0xFF;
              rgba[di + 3] = 0xFF;
            }
          else
            {
              rgba[di] = 0x00;
              rgba[di + 1] = 0x00;
              rgba[di + 2] = 0x00;
              rgba[di + 3] = 0xFF;
            }
        }
    }
  return write_png (png_path, rgba, ow, oh, 4);
}

bool
write_rgba_png (const std::vector<uint8_t> &rgba_32x32,
                const std::string &png_path, int scale)
{
  if (rgba_32x32.size () != 32 * 32 * 4)
    return false;
  int ow = 32 * scale;
  int oh = 32 * scale;
  std::vector<uint8_t> rgba (ow * oh * 4);
  for (int y = 0; y < oh; y++)
    {
      for (int x = 0; x < ow; x++)
        {
          int si = ((y / scale) * 32 + (x / scale)) * 4;
          int di = (y * ow + x) * 4;
          for (int c = 0; c < 4; c++)
            rgba[di + c] = rgba_32x32[si + c];
        }
    }
  return write_png (png_path, rgba, ow, oh, 4);
}
