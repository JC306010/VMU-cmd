#include "vmu_image.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

// ---- RootBlock ----

void
RootBlock::parse (const uint8_t data[VMU_BLOCK_SIZE])
{
  bool has_magic = true;
  for (int i = 0; i < 16; i++)
    {
      if (data[i] != 0x55)
        {
          has_magic = false;
          break;
        }
    }

  custom_color = (data[0x10] != 0);
  color_blue = data[0x11];
  color_green = data[0x12];
  color_red = data[0x13];
  color_alpha = data[0x14];

  for (int i = 0; i < 8; i++)
    timestamp[i] = data[0x30 + i];

  volume_last = data[0x40] | (data[0x41] << 8);
  partition = data[0x42] | (data[0x43] << 8);
  root = data[0x44] | (data[0x45] << 8);
  fat_first = data[0x46] | (data[0x47] << 8);
  fat_size = data[0x48] | (data[0x49] << 8);
  dir_last = data[0x4A] | (data[0x4B] << 8);
  dir_size = data[0x4C] | (data[0x4D] << 8);
  icon_shape = data[0x4E];
  // sort byte at 0x4F
  hidden_first = data[0x50] | (data[0x51] << 8);
  hidden_size = data[0x52] | (data[0x53] << 8);
  game_first = data[0x54] | (data[0x55] << 8);
  game_size = data[0x56] | (data[0x57] << 8);
}

void
RootBlock::serialize (uint8_t data[VMU_BLOCK_SIZE]) const
{
  std::memset (data, 0, VMU_BLOCK_SIZE);

  for (int i = 0; i < 16; i++)
    data[i] = 0x55;

  data[0x10] = custom_color ? 1 : 0;
  data[0x11] = color_blue;
  data[0x12] = color_green;
  data[0x13] = color_red;
  data[0x14] = color_alpha;

  for (int i = 0; i < 8; i++)
    data[0x30 + i] = timestamp[i];

  data[0x40] = volume_last & 0xFF;
  data[0x41] = (volume_last >> 8) & 0xFF;
  data[0x42] = partition & 0xFF;
  data[0x43] = (partition >> 8) & 0xFF;
  data[0x44] = root & 0xFF;
  data[0x45] = (root >> 8) & 0xFF;
  data[0x46] = fat_first & 0xFF;
  data[0x47] = (fat_first >> 8) & 0xFF;
  data[0x48] = fat_size & 0xFF;
  data[0x49] = (fat_size >> 8) & 0xFF;
  data[0x4A] = dir_last & 0xFF;
  data[0x4B] = (dir_last >> 8) & 0xFF;
  data[0x4C] = dir_size & 0xFF;
  data[0x4D] = (dir_size >> 8) & 0xFF;
  data[0x4E] = icon_shape;
  // 0x4F sort byte (0)
  data[0x50] = hidden_first & 0xFF;
  data[0x51] = (hidden_first >> 8) & 0xFF;
  data[0x52] = hidden_size & 0xFF;
  data[0x53] = (hidden_size >> 8) & 0xFF;
  data[0x54] = game_first & 0xFF;
  data[0x55] = (game_first >> 8) & 0xFF;
  data[0x56] = game_size & 0xFF;
  data[0x57] = (game_size >> 8) & 0xFF;
}

static std::string
bcd_to_string (const uint8_t bcd[8])
{
  char buf[64];
  int century = (bcd[0] >> 4) * 10 + (bcd[0] & 0xF);
  int year = (bcd[1] >> 4) * 10 + (bcd[1] & 0xF);
  int month = (bcd[2] >> 4) * 10 + (bcd[2] & 0xF);
  int day = (bcd[3] >> 4) * 10 + (bcd[3] & 0xF);
  int hour = (bcd[4] >> 4) * 10 + (bcd[4] & 0xF);
  int minute = (bcd[5] >> 4) * 10 + (bcd[5] & 0xF);
  int second = (bcd[6] >> 4) * 10 + (bcd[6] & 0xF);
  int full_year = century * 100 + year;
  if (full_year < 1980)
    full_year += 2000;
  std::snprintf (buf, sizeof (buf), "%04d-%02d-%02d %02d:%02d:%02d", full_year,
                 month, day, hour, minute, second);
  return buf;
}

std::string
RootBlock::format_timestamp () const
{
  return bcd_to_string (timestamp);
}

// ---- DirEntry ----

std::string
DirEntry::name () const
{
  std::string s (filename, 12);
  // trim trailing spaces and nulls
  while (!s.empty () && (s.back () == ' ' || s.back () == '\0'))
    s.pop_back ();
  return s;
}

std::string
DirEntry::type_str () const
{
  switch (type)
    {
    case FILE_TYPE_DATA:
      return "DATA";
    case FILE_TYPE_GAME:
      return "GAME";
    default:
      return "NONE";
    }
}

std::string
DirEntry::format_timestamp () const
{
  return bcd_to_string (timestamp);
}

void
DirEntry::parse (const uint8_t data[VMU_DIR_ENTRY_SIZE])
{
  type = data[0];
  copy_protect = data[1];
  first_block = data[2] | (data[3] << 8);
  for (int i = 0; i < 12; i++)
    filename[i] = data[4 + i];
  for (int i = 0; i < 8; i++)
    timestamp[i] = data[0x10 + i];
  size_blocks = data[0x18] | (data[0x19] << 8);
  header_offset = data[0x1A] | (data[0x1B] << 8);
}

void
DirEntry::serialize (uint8_t data[VMU_DIR_ENTRY_SIZE]) const
{
  std::memset (data, 0, VMU_DIR_ENTRY_SIZE);
  data[0] = type;
  data[1] = copy_protect;
  data[2] = first_block & 0xFF;
  data[3] = (first_block >> 8) & 0xFF;
  for (int i = 0; i < 12; i++)
    data[4 + i] = filename[i];
  for (int i = 0; i < 8; i++)
    data[0x10 + i] = timestamp[i];
  data[0x18] = size_blocks & 0xFF;
  data[0x19] = (size_blocks >> 8) & 0xFF;
  data[0x1A] = header_offset & 0xFF;
  data[0x1B] = (header_offset >> 8) & 0xFF;
}

// ---- VMUImage ----

VMUImage::VMUImage (const std::string &path) { load (path); }

bool
VMUImage::load (const std::string &path)
{
  std::ifstream f (path, std::ios::binary);
  if (!f)
    return false;
  f.read (reinterpret_cast<char *> (data_.data ()), VMU_IMAGE_SIZE);
  if (!f)
    return false;

  parse_fat ();
  parse_directory ();

  uint8_t root_block[VMU_BLOCK_SIZE];
  read_block (VMU_ROOT_BLOCK, root_block);
  root_.parse (root_block);

  loaded_ = true;
  return true;
}

bool
VMUImage::save (const std::string &path) const
{
  std::ofstream f (path, std::ios::binary);
  if (!f)
    return false;
  f.write (reinterpret_cast<const char *> (data_.data ()), VMU_IMAGE_SIZE);
  return f.good ();
}

void
VMUImage::read_block (int block, uint8_t *out) const
{
  std::memcpy (out, &data_[block * VMU_BLOCK_SIZE], VMU_BLOCK_SIZE);
}

void
VMUImage::write_block (int block, const uint8_t *data)
{
  std::memcpy (&data_[block * VMU_BLOCK_SIZE], data, VMU_BLOCK_SIZE);
}

void
VMUImage::parse_fat ()
{
  std::memset (fat_.data (), 0, sizeof (fat_));
  int fat_start_block = root_.fat_first;
  if (fat_start_block == 0)
    fat_start_block = VMU_FAT_BLOCK;
  int fat_size = root_.fat_size;
  if (fat_size == 0)
    fat_size = 1;

  for (int b = 0; b < fat_size; b++)
    {
      uint8_t block_data[VMU_BLOCK_SIZE];
      read_block (fat_start_block + b, block_data);
      int base = b * (VMU_BLOCK_SIZE / 2);
      for (int i = 0; i < VMU_BLOCK_SIZE / 2 && (base + i) < VMU_NUM_BLOCKS;
           i++)
        {
          fat_[base + i] = block_data[i * 2] | (block_data[i * 2 + 1] << 8);
        }
    }
}

void
VMUImage::build_fat ()
{
  int fat_start = root_.fat_first;
  int fat_size = root_.fat_size;
  if (fat_size == 0)
    fat_size = 1;

  for (int b = 0; b < fat_size; b++)
    {
      uint8_t block_data[VMU_BLOCK_SIZE];
      std::memset (block_data, 0, VMU_BLOCK_SIZE);
      int base = b * (VMU_BLOCK_SIZE / 2);
      for (int i = 0; i < VMU_BLOCK_SIZE / 2 && (base + i) < VMU_NUM_BLOCKS;
           i++)
        {
          block_data[i * 2] = fat_[base + i] & 0xFF;
          block_data[i * 2 + 1] = (fat_[base + i] >> 8) & 0xFF;
        }
      write_block (fat_start + b, block_data);
    }
}

void
VMUImage::parse_directory ()
{
  files_.clear ();
  int dir_last = root_.dir_last;
  int dir_size = root_.dir_size;
  if (dir_last == 0)
    {
      dir_last = VMU_DIR_LAST;
      dir_size = VMU_DIR_SIZE;
    }

  int dir_first = dir_last - dir_size + 1;

  for (int b = 0; b < dir_size; b++)
    {
      int block_num = dir_first + b;
      uint8_t block_data[VMU_BLOCK_SIZE];
      read_block (block_num, block_data);
      for (int e = 0; e < VMU_DIR_ENTRIES_PER_BLOCK; e++)
        {
          DirEntry entry;
          entry.parse (&block_data[e * VMU_DIR_ENTRY_SIZE]);
          if (entry.is_valid ())
            {
              files_.push_back (entry);
            }
        }
    }
}

void
VMUImage::build_directory ()
{
  int dir_last = root_.dir_last;
  int dir_size = root_.dir_size;
  if (dir_last == 0)
    {
      dir_last = VMU_DIR_LAST;
      dir_size = VMU_DIR_SIZE;
    }
  int dir_first = dir_last - dir_size + 1;

  // Clear all directory blocks
  for (int b = 0; b < dir_size; b++)
    {
      uint8_t block_data[VMU_BLOCK_SIZE];
      std::memset (block_data, 0, VMU_BLOCK_SIZE);
      write_block (dir_first + b, block_data);
    }

  // Write entries (growing from first directory block upward)
  int total_entries = files_.size ();
  int entry_idx = 0;
  for (int b = 0; b < dir_size && entry_idx < total_entries; b++)
    {
      uint8_t block_data[VMU_BLOCK_SIZE];
      read_block (dir_first + b, block_data);
      for (int e = 0;
           e < VMU_DIR_ENTRIES_PER_BLOCK && entry_idx < total_entries; e++)
        {
          files_[entry_idx].serialize (&block_data[e * VMU_DIR_ENTRY_SIZE]);
          entry_idx++;
        }
      write_block (dir_first + b, block_data);
    }
}

std::vector<DirEntry>
VMUImage::list_files () const
{
  return files_;
}

bool
VMUImage::extract_file (const std::string &name,
                        std::vector<uint8_t> &out_data) const
{
  auto it
      = std::find_if (files_.begin (), files_.end (),
                      [&] (const DirEntry &e) { return e.name () == name; });
  if (it == files_.end ())
    return false;

  const DirEntry &entry = *it;
  int total_bytes = entry.size_blocks * VMU_BLOCK_SIZE;
  out_data.resize (total_bytes);

  int block = entry.first_block;
  int offset = 0;
  while (block >= 0 && block < VMU_NUM_BLOCKS && offset < total_bytes)
    {
      uint8_t block_data[VMU_BLOCK_SIZE];
      read_block (block, block_data);
      int copy_size = std::min (VMU_BLOCK_SIZE, total_bytes - offset);
      std::memcpy (&out_data[offset], block_data, copy_size);
      offset += VMU_BLOCK_SIZE;

      uint16_t next = fat_[block];
      if (next == FAT_LAST_IN_FILE)
        break;
      if (next >= VMU_NUM_BLOCKS || next == FAT_UNALLOCATED
          || next == FAT_DAMAGED)
        break;
      block = next;
    }

  return true;
}

bool
VMUImage::add_file (const std::string &name, const std::vector<uint8_t> &data,
                    uint8_t type)
{
  // Check if file already exists
  auto it
      = std::find_if (files_.begin (), files_.end (),
                      [&] (const DirEntry &e) { return e.name () == name; });
  if (it != files_.end ())
    return false;

  // Pad name to 12 chars
  char filename[12];
  std::memset (filename, ' ', 12);
  for (size_t i = 0; i < name.size () && i < 12; i++)
    {
      filename[i] = name[i];
    }

  int num_blocks = (data.size () + VMU_BLOCK_SIZE - 1) / VMU_BLOCK_SIZE;
  if (num_blocks == 0)
    num_blocks = 1;

  // Find free blocks (allocate from high to low for data files)
  std::vector<int> allocated;
  if (type == FILE_TYPE_GAME)
    {
      // Game files start at block 0 and grow contiguously
      for (int b = 0; b < VMU_NUM_USER_BLOCKS; b++)
        {
          if (fat_[b] == FAT_UNALLOCATED || fat_[b] == FAT_DAMAGED)
            {
              allocated.push_back (b);
              if ((int)allocated.size () == num_blocks)
                break;
            }
          else
            break;
        }
    }
  else
    {
      // Data files use highest available blocks
      for (int b = VMU_NUM_USER_BLOCKS - 1; b >= 0; b--)
        {
          if (fat_[b] == FAT_UNALLOCATED || fat_[b] == FAT_DAMAGED)
            {
              allocated.push_back (b);
              if ((int)allocated.size () == num_blocks)
                break;
            }
        }
      // Blocks are collected high-to-low, but file data should be written in
      // order
      std::reverse (allocated.begin (), allocated.end ());
    }

  if ((int)allocated.size () < num_blocks)
    return false;

  // Write data to blocks
  for (int i = 0; i < num_blocks; i++)
    {
      int block = allocated[i];
      uint8_t block_data[VMU_BLOCK_SIZE];
      std::memset (block_data, 0, VMU_BLOCK_SIZE);
      int offset = i * VMU_BLOCK_SIZE;
      int copy_size = std::min (VMU_BLOCK_SIZE, (int)data.size () - offset);
      if (copy_size > 0)
        {
          std::memcpy (block_data, &data[offset], copy_size);
        }
      write_block (block, block_data);
    }

  // Update FAT
  for (int i = 0; i < num_blocks; i++)
    {
      if (i < num_blocks - 1)
        {
          fat_[allocated[i]] = allocated[i + 1];
        }
      else
        {
          fat_[allocated[i]] = FAT_LAST_IN_FILE;
        }
    }

  // Create directory entry
  DirEntry entry;
  entry.type = type;
  entry.copy_protect = 0;
  entry.first_block = allocated[0];
  std::memcpy (entry.filename, filename, 12);
  // Set timestamp (current date in BCD - simplified)
  entry.timestamp[0] = 0x20; // century 20
  entry.timestamp[1] = 0x26; // year 26
  entry.timestamp[2] = 0x05; // month 05
  entry.timestamp[3] = 0x27; // day 27
  entry.timestamp[4] = 0x12; // hour
  entry.timestamp[5] = 0x00; // minute
  entry.timestamp[6] = 0x00; // second
  entry.timestamp[7] = 0x02; // day of week
  entry.size_blocks = num_blocks;
  entry.header_offset = 0;

  files_.push_back (entry);

  // Rebuild directory and FAT in image
  build_directory ();
  build_fat ();

  // Update root block
  uint8_t root_data[VMU_BLOCK_SIZE];
  root_.serialize (root_data);
  write_block (VMU_ROOT_BLOCK, root_data);

  return true;
}

bool
VMUImage::delete_file (const std::string &name)
{
  auto it
      = std::find_if (files_.begin (), files_.end (),
                      [&] (const DirEntry &e) { return e.name () == name; });
  if (it == files_.end ())
    return false;

  // Free FAT chains
  int block = it->first_block;
  while (block >= 0 && block < VMU_NUM_BLOCKS)
    {
      uint16_t next = fat_[block];
      fat_[block] = FAT_UNALLOCATED;
      if (next == FAT_LAST_IN_FILE)
        break;
      if (next >= VMU_NUM_BLOCKS)
        break;
      block = next;
    }

  files_.erase (it);

  build_directory ();
  build_fat ();

  uint8_t root_data[VMU_BLOCK_SIZE];
  root_.serialize (root_data);
  write_block (VMU_ROOT_BLOCK, root_data);

  return true;
}

bool
VMUImage::set_icon_shape (uint8_t shape)
{
  if (shape > 123)
    return false;
  root_.icon_shape = shape;

  uint8_t root_data[VMU_BLOCK_SIZE];
  root_.serialize (root_data);
  write_block (VMU_ROOT_BLOCK, root_data);
  return true;
}

bool
VMUImage::set_custom_color (uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
  root_.custom_color = true;
  root_.color_red = r;
  root_.color_green = g;
  root_.color_blue = b;
  root_.color_alpha = a;

  uint8_t root_data[VMU_BLOCK_SIZE];
  root_.serialize (root_data);
  write_block (VMU_ROOT_BLOCK, root_data);
  return true;
}

bool
VMUImage::transfer_file (const std::string &name, VMUImage &dst) const
{
  std::vector<uint8_t> data;
  if (!extract_file (name, data))
    return false;

  auto it
      = std::find_if (files_.begin (), files_.end (),
                      [&] (const DirEntry &e) { return e.name () == name; });
  if (it == files_.end ())
    return false;

  return dst.add_file (name, data, it->type);
}

bool
VMUImage::rebuild_fat ()
{
  // Reset all user blocks to unallocated
  for (int i = 0; i < VMU_NUM_BLOCKS; i++)
    {
      if (i == VMU_ROOT_BLOCK || i == root_.fat_first)
        continue;
      int dir_last = root_.dir_last;
      int dir_size = root_.dir_size;
      int dir_first = dir_last - dir_size + 1;
      if (i >= dir_first && i <= dir_last)
        continue;

      fat_[i] = FAT_UNALLOCATED;
    }

  // Mark system blocks
  // Root
  fat_[VMU_ROOT_BLOCK] = FAT_LAST_IN_FILE;

  // FAT blocks (chain)
  int fat_start = root_.fat_first;
  int fat_size = root_.fat_size;
  for (int b = 0; b < fat_size; b++)
    {
      if (b < fat_size - 1)
        fat_[fat_start + b] = fat_start + b + 1;
      else
        fat_[fat_start + b] = FAT_LAST_IN_FILE;
    }

  // Directory blocks (chain from first to last)
  int dir_last = root_.dir_last;
  int dir_size = root_.dir_size;
  int dir_first = dir_last - dir_size + 1;
  for (int b = 0; b < dir_size; b++)
    {
      if (b < dir_size - 1)
        fat_[dir_first + b] = dir_first + b + 1;
      else
        fat_[dir_first + b] = FAT_LAST_IN_FILE;
    }

  // Allocate blocks for each file
  for (auto &entry : files_)
    {
      int block = entry.first_block;
      int blocks_to_alloc = entry.size_blocks;
      int count = 0;

      // Scan for free blocks
      std::vector<int> allocated;
      if (entry.type == FILE_TYPE_GAME)
        {
          for (int b = 0; b < VMU_NUM_USER_BLOCKS && count < blocks_to_alloc;
               b++)
            {
              if (fat_[b] == FAT_UNALLOCATED)
                {
                  allocated.push_back (b);
                  count++;
                }
            }
        }
      else
        {
          for (int b = VMU_NUM_USER_BLOCKS - 1;
               b >= 0 && count < blocks_to_alloc; b--)
            {
              if (fat_[b] == FAT_UNALLOCATED)
                {
                  allocated.push_back (b);
                  count++;
                }
            }
          std::reverse (allocated.begin (), allocated.end ());
        }

      if (count < blocks_to_alloc)
        return false;

      // Update FAT
      for (int i = 0; i < blocks_to_alloc; i++)
        {
          if (i < blocks_to_alloc - 1)
            fat_[allocated[i]] = allocated[i + 1];
          else
            fat_[allocated[i]] = FAT_LAST_IN_FILE;
        }

      entry.first_block = allocated[0];
    }

  build_fat ();
  build_directory ();

  uint8_t root_data[VMU_BLOCK_SIZE];
  root_.serialize (root_data);
  write_block (VMU_ROOT_BLOCK, root_data);

  return true;
}
