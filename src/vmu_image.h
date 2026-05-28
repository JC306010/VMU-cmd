#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

static constexpr int VMU_BLOCK_SIZE = 512;
static constexpr int VMU_NUM_BLOCKS = 256;
static constexpr int VMU_IMAGE_SIZE = VMU_BLOCK_SIZE * VMU_NUM_BLOCKS; // 128KB

static constexpr int VMU_ROOT_BLOCK = 255;
static constexpr int VMU_FAT_BLOCK = 254;
static constexpr int VMU_DIR_LAST = 253;
static constexpr int VMU_DIR_SIZE = 13;
static constexpr int VMU_NUM_USER_BLOCKS = 200;
static constexpr int VMU_HIDDEN_FIRST = 200;
static constexpr int VMU_HIDDEN_SIZE = 31;

static constexpr int VMU_DIR_ENTRY_SIZE = 32;
static constexpr int VMU_DIR_ENTRIES_PER_BLOCK
    = VMU_BLOCK_SIZE / VMU_DIR_ENTRY_SIZE;

// FAT entry values
static constexpr uint16_t FAT_UNALLOCATED = 0xFFFC;
static constexpr uint16_t FAT_DAMAGED = 0xFFFF;
static constexpr uint16_t FAT_LAST_IN_FILE = 0xFFFA;

// File types
static constexpr uint8_t FILE_TYPE_NONE = 0x00;
static constexpr uint8_t FILE_TYPE_DATA = 0x33;
static constexpr uint8_t FILE_TYPE_GAME = 0xCC;

struct RootBlock
{
  bool custom_color = false;
  uint8_t color_blue = 0;
  uint8_t color_green = 0;
  uint8_t color_red = 0;
  uint8_t color_alpha = 0;
  uint8_t timestamp[8] = {};
  uint16_t volume_last = 255;
  uint16_t partition = 0;
  uint16_t root = 255;
  uint16_t fat_first = 254;
  uint16_t fat_size = 1;
  uint16_t dir_last = 253;
  uint16_t dir_size = 13;
  uint8_t icon_shape = 0;
  uint16_t hidden_first = 200;
  uint16_t hidden_size = 31;
  uint16_t game_first = 0;
  uint16_t game_size = 128;

  void parse (const uint8_t data[VMU_BLOCK_SIZE]);
  void serialize (uint8_t data[VMU_BLOCK_SIZE]) const;
  std::string format_timestamp () const;
};

struct DirEntry
{
  uint8_t type = FILE_TYPE_NONE;
  uint8_t copy_protect = 0;
  uint16_t first_block = 0;
  char filename[12] = {};
  uint8_t timestamp[8] = {};
  uint16_t size_blocks = 0;
  uint16_t header_offset = 0;

  bool
  is_valid () const
  {
    return type == FILE_TYPE_DATA || type == FILE_TYPE_GAME;
  }
  std::string name () const;
  std::string type_str () const;
  std::string format_timestamp () const;
  int
  size_bytes () const
  {
    return size_blocks * VMU_BLOCK_SIZE;
  }

  void parse (const uint8_t data[VMU_DIR_ENTRY_SIZE]);
  void serialize (uint8_t data[VMU_DIR_ENTRY_SIZE]) const;
};

class VMUImage
{
public:
  VMUImage () = default;
  explicit VMUImage (const std::string &path);

  bool load (const std::string &path);
  bool save (const std::string &path) const;

  const RootBlock &
  root () const
  {
    return root_;
  }
  const std::vector<DirEntry> &
  files () const
  {
    return files_;
  }
  const std::array<uint8_t, VMU_IMAGE_SIZE> &
  raw () const
  {
    return data_;
  }

  bool
  is_loaded () const
  {
    return loaded_;
  }

  // File operations
  std::vector<DirEntry> list_files () const;
  bool extract_file (const std::string &name,
                     std::vector<uint8_t> &out_data) const;
  bool add_file (const std::string &name, const std::vector<uint8_t> &data,
                 uint8_t type = FILE_TYPE_DATA);
  bool delete_file (const std::string &name);

  // Icon shape and color
  bool set_icon_shape (uint8_t shape);
  bool set_custom_color (uint8_t r, uint8_t g, uint8_t b, uint8_t a);

  // Transfer
  bool transfer_file (const std::string &name, VMUImage &dst) const;

  // Internal repair
  bool rebuild_fat ();

private:
  void parse_fat ();
  void parse_directory ();
  void build_fat ();
  void build_directory ();

  // Get/set block from raw data
  void read_block (int block, uint8_t *out) const;
  void write_block (int block, const uint8_t *data);

  std::array<uint8_t, VMU_IMAGE_SIZE> data_{};
  RootBlock root_;
  std::vector<DirEntry> files_;
  std::array<uint16_t, VMU_NUM_BLOCKS> fat_{};
  bool loaded_ = false;
};
