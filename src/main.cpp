#include "icon_converter.h"
#include "vmu_image.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

static void
print_usage (const char *prog)
{
  std::cerr << "VMU Explorer v1.0 - Sega Dreamcast Visual Memory Unit tool\n";
  std::cerr << "Usage:\n";
  std::cerr << "  " << prog << " list <vmu_image.bin>\n";
  std::cerr << "  " << prog << " info <vmu_image.bin>\n";
  std::cerr << "  " << prog
            << " extract <vmu_image.bin> <file_name> [output_file]\n";
  std::cerr << "  " << prog
            << " add <vmu_image.bin> <file_name> <input_file>\n";
  std::cerr << "  " << prog
            << " addgame <vmu_image.bin> <file_name> <input_file>\n";
  std::cerr << "  " << prog << " delete <vmu_image.bin> <file_name>\n";
  std::cerr << "  " << prog << " icon-export <vmu_image.bin> <output_prefix>\n";
  std::cerr << "  " << prog << " icon-import <vmu_image.bin> <input.png>\n";
  std::cerr << "  " << prog << " icon-set-shape <vmu_image.bin> <0-123>\n";
  std::cerr << "  " << prog
            << " icon-set-color <vmu_image.bin> <R> <G> <B> <A>\n";
  std::cerr << "  " << prog
            << " transfer <src_image.bin> <dst_image.bin> <file_name>\n";
  std::cerr << "  " << prog << " rebuild <vmu_image.bin>\n";
}

static int
cmd_list (VMUImage &vmu)
{
  auto files = vmu.list_files ();
  std::cout << "Files on VMU:\n";
  if (files.empty ())
    {
      std::cout << "  (empty)\n";
      return 0;
    }
  for (const auto &f : files)
    {
      std::cout << "  " << std::setw (12) << std::left << f.name () << "  "
                << f.type_str () << "  " << std::setw (4) << f.size_blocks
                << " block(s)" << "  block " << std::setw (3) << f.first_block
                << "  " << f.format_timestamp () << "\n";
    }
  return 0;
}

static int
cmd_info (VMUImage &vmu)
{
  const auto &r = vmu.root ();
  std::cout << "VMU Info:\n";
  std::cout << "  Formatted: " << r.format_timestamp () << "\n";
  std::cout << "  Color: RGBA(" << (int)r.color_red << "," << (int)r.color_green
            << "," << (int)r.color_blue << "," << (int)r.color_alpha << ")\n";
  std::cout << "  Icon shape: " << (int)r.icon_shape << "\n";
  std::cout << "  Total blocks: " << (r.volume_last + 1) << "\n";
  std::cout << "  User blocks: " << VMU_NUM_USER_BLOCKS << "\n";
  std::cout << "  Root block: " << r.root << "\n";
  std::cout << "  FAT block: " << r.fat_first << " (size: " << r.fat_size
            << ")\n";
  std::cout << "  DIR block: " << r.dir_last << " (size: " << r.dir_size
            << ")\n";
  std::cout << "  Hidden: " << r.hidden_first << "-"
            << (r.hidden_first + r.hidden_size - 1) << "\n";
  std::cout << "  Game area: " << r.game_first << "-"
            << (r.game_first + r.game_size - 1) << "\n";
  return 0;
}

static int
cmd_extract (VMUImage &vmu, const std::string &name, const std::string &output)
{
  std::vector<uint8_t> data;
  if (!vmu.extract_file (name, data))
    {
      std::cerr << "Error: file '" << name << "' not found\n";
      return 1;
    }

  std::string out_path = output.empty () ? name : output;
  std::ofstream f (out_path, std::ios::binary);
  if (!f)
    {
      std::cerr << "Error: cannot write '" << out_path << "'\n";
      return 1;
    }
  f.write (reinterpret_cast<const char *> (data.data ()), data.size ());
  std::cout << "Extracted " << data.size () << " bytes to " << out_path << "\n";
  return 0;
}

static int
cmd_add (VMUImage &vmu, const std::string &name, const std::string &input,
         uint8_t type)
{
  std::ifstream f (input, std::ios::binary | std::ios::ate);
  if (!f)
    {
      std::cerr << "Error: cannot read '" << input << "'\n";
      return 1;
    }
  size_t size = f.tellg ();
  f.seekg (0);
  std::vector<uint8_t> data (size);
  f.read (reinterpret_cast<char *> (data.data ()), size);

  if (!vmu.add_file (name, data, type))
    {
      std::cerr << "Error: failed to add file '" << name
                << "' (may already exist, or not enough space)\n";
      return 1;
    }
  std::cout << "Added '" << name << "' (" << size << " bytes)\n";
  return 0;
}

static int
cmd_delete (VMUImage &vmu, const std::string &name)
{
  if (!vmu.delete_file (name))
    {
      std::cerr << "Error: file '" << name << "' not found\n";
      return 1;
    }
  std::cout << "Deleted '" << name << "'\n";
  return 0;
}

static int
cmd_icon_export (VMUImage &vmu, const std::string &prefix)
{
  // Check if ICONDATA_VMS file exists
  auto files = vmu.list_files ();
  bool has_icondata = false;
  for (const auto &f : files)
    {
      if (f.name () == "ICONDATA_VMS")
        {
          has_icondata = true;
          break;
        }
    }

  if (!has_icondata)
    {
      const auto &r = vmu.root ();
      std::cout << "No custom ICONDATA_VMS file found on this VMU.\n";
      std::cout << "The volume icon is defined by:\n";
      std::cout << "  Icon shape: " << (int)r.icon_shape
                << " (0-123, built-in BIOS shapes)\n";
      std::cout << "  Custom color: "
                << (r.custom_color ? "enabled" : "disabled") << "\n";
      std::cout << "  RGBA(" << (int)r.color_red << "," << (int)r.color_green
                << "," << (int)r.color_blue << "," << (int)r.color_alpha
                << ")\n";
      std::cout << "Use icon-set-shape and icon-set-color to modify these,\n";
      std::cout
          << "or icon-import to add an ICONDATA_VMS file for a custom icon.\n";
      return 0;
    }

  // Extract ICONDATA_VMS
  std::vector<uint8_t> raw;
  if (!vmu.extract_file ("ICONDATA_VMS", raw))
    {
      std::cerr << "Error: failed to extract ICONDATA_VMS\n";
      return 1;
    }

  // Decode
  std::vector<uint8_t> mono, color;
  std::string desc;
  if (!decode_icondata_vms_full (raw, mono, color, desc))
    {
      std::cerr << "Error: failed to decode ICONDATA_VMS\n";
      return 1;
    }

  std::string mono_path = prefix + "_mono.png";
  if (!write_grayscale_png (mono, mono_path, 4))
    {
      std::cerr << "Error: failed to write " << mono_path << "\n";
      return 1;
    }
  std::cout << "Exported mono icon: " << mono_path << "\n";

  if (!color.empty ())
    {
      std::string color_path = prefix + "_color.png";
      if (!write_rgba_png (color, color_path, 4))
        {
          std::cerr << "Error: failed to write " << color_path << "\n";
          return 1;
        }
      std::cout << "Exported color icon: " << color_path << "\n";
    }

  if (!desc.empty ())
    {
      std::cout << "Description: " << desc << "\n";
    }

  return 0;
}

static int
cmd_icon_import (VMUImage &vmu, const std::string &png_path)
{
  // Encode PNG as ICONDATA_VMS
  std::vector<uint8_t> icondata;
  if (!create_icondata_vms (png_path, icondata))
    {
      std::cerr << "Error: failed to read/encode " << png_path << "\n";
      return 1;
    }

  // Remove old ICONDATA_VMS if present
  vmu.delete_file ("ICONDATA_VMS");

  // Add new ICONDATA_VMS
  if (!vmu.add_file ("ICONDATA_VMS", icondata, FILE_TYPE_DATA))
    {
      std::cerr
          << "Error: failed to add ICONDATA_VMS to VMU (not enough space?)\n";
      return 1;
    }

  std::cout << "Icon imported from " << png_path << "\n";
  return 0;
}

static int
cmd_icon_set_shape (VMUImage &vmu, const std::string &shape_str)
{
  int shape = std::stoi (shape_str);
  if (shape < 0 || shape > 123)
    {
      std::cerr << "Error: icon shape must be 0-123\n";
      return 1;
    }
  if (!vmu.set_icon_shape (static_cast<uint8_t> (shape)))
    {
      std::cerr << "Error: failed to set icon shape\n";
      return 1;
    }
  std::cout << "Icon shape set to " << shape << "\n";
  return 0;
}

static int
cmd_icon_set_color (VMUImage &vmu, const std::string &r_str,
                    const std::string &g_str, const std::string &b_str,
                    const std::string &a_str)
{
  int r = std::stoi (r_str);
  int g = std::stoi (g_str);
  int b = std::stoi (b_str);
  int a = std::stoi (a_str);
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0
      || a > 255)
    {
      std::cerr << "Error: color values must be 0-255\n";
      return 1;
    }
  if (!vmu.set_custom_color (r, g, b, a))
    {
      std::cerr << "Error: failed to set custom color\n";
      return 1;
    }
  std::cout << "Custom color set to RGBA(" << r << "," << g << "," << b << ","
            << a << ")\n";
  return 0;
}

static int
cmd_transfer (const std::string &src_path, const std::string &dst_path,
              const std::string &name)
{
  VMUImage src (src_path);
  if (!src.is_loaded ())
    {
      std::cerr << "Error: cannot open source '" << src_path << "'\n";
      return 1;
    }

  VMUImage dst (dst_path);
  if (!dst.is_loaded ())
    {
      std::cerr << "Error: cannot open destination '" << dst_path << "'\n";
      return 1;
    }

  if (!src.transfer_file (name, dst))
    {
      std::cerr << "Error: transfer failed (file not found or no space)\n";
      return 1;
    }

  if (!dst.save (dst_path))
    {
      std::cerr << "Error: failed to save destination\n";
      return 1;
    }

  std::cout << "Transferred '" << name << "' from " << src_path << " to "
            << dst_path << "\n";
  return 0;
}

static int
cmd_rebuild (VMUImage &vmu)
{
  if (!vmu.rebuild_fat ())
    {
      std::cerr << "Error: rebuild failed (not enough free blocks)\n";
      return 1;
    }
  std::cout << "FAT and directory rebuilt\n";
  return 0;
}

int
main (int argc, char **argv)
{
  if (argc < 2)
    {
      print_usage (argv[0]);
      return 1;
    }

  std::string cmd = argv[1];

  if (cmd == "transfer")
    {
      if (argc < 5)
        {
          std::cerr << "Usage: " << argv[0] << " transfer <src> <dst> <file>\n";
          return 1;
        }
      int ret = cmd_transfer (argv[2], argv[3], argv[4]);

      return ret;
    }

  if (argc < 3)
    {
      print_usage (argv[0]);
      return 1;
    }

  std::string image_path = argv[2];
  VMUImage vmu (image_path);
  if (!vmu.is_loaded ())
    {
      std::cerr << "Error: cannot open VMU image '" << image_path << "'\n";
      return 1;
    }

  int ret = 0;

  if (cmd == "list")
    {
      ret = cmd_list (vmu);
    }
  else if (cmd == "info")
    {
      ret = cmd_info (vmu);
    }
  else if (cmd == "extract")
    {
      std::string name = argc > 3 ? argv[3] : "";
      std::string output = argc > 4 ? argv[4] : "";
      if (name.empty ())
        {
          std::cerr << "Usage: " << argv[0]
                    << " extract <image> <name> [output]\n";
          return 1;
        }
      ret = cmd_extract (vmu, name, output);
    }
  else if (cmd == "add")
    {
      if (argc < 5)
        {
          std::cerr << "Usage: " << argv[0] << " add <image> <name> <input>\n";
          return 1;
        }
      ret = cmd_add (vmu, argv[3], argv[4], FILE_TYPE_DATA);
    }
  else if (cmd == "addgame")
    {
      if (argc < 5)
        {
          std::cerr << "Usage: " << argv[0]
                    << " addgame <image> <name> <input>\n";
          return 1;
        }
      ret = cmd_add (vmu, argv[3], argv[4], FILE_TYPE_GAME);
    }
  else if (cmd == "delete")
    {
      if (argc < 4)
        {
          std::cerr << "Usage: " << argv[0] << " delete <image> <name>\n";
          return 1;
        }
      ret = cmd_delete (vmu, argv[3]);
    }
  else if (cmd == "icon-export")
    {
      if (argc < 4)
        {
          std::cerr << "Usage: " << argv[0]
                    << " icon-export <image> <output_prefix>\n";
          return 1;
        }
      ret = cmd_icon_export (vmu, argv[3]);
    }
  else if (cmd == "icon-import")
    {
      if (argc < 4)
        {
          std::cerr << "Usage: " << argv[0]
                    << " icon-import <image> <input.png>\n";
          return 1;
        }
      ret = cmd_icon_import (vmu, argv[3]);
    }
  else if (cmd == "icon-set-shape")
    {
      if (argc < 4)
        {
          std::cerr << "Usage: " << argv[0]
                    << " icon-set-shape <image> <0-123>\n";
          return 1;
        }
      ret = cmd_icon_set_shape (vmu, argv[3]);
    }
  else if (cmd == "icon-set-color")
    {
      if (argc < 7)
        {
          std::cerr << "Usage: " << argv[0]
                    << " icon-set-color <image> <R> <G> <B> <A>\n";
          return 1;
        }
      ret = cmd_icon_set_color (vmu, argv[3], argv[4], argv[5], argv[6]);
    }
  else if (cmd == "rebuild")
    {
      ret = cmd_rebuild (vmu);
    }
  else
    {
      std::cerr << "Unknown command: " << cmd << "\n";
      print_usage (argv[0]);
      return 1;
    }

  // Save changes if we modified the image
  if (ret == 0
      && (cmd == "add" || cmd == "addgame" || cmd == "delete"
          || cmd == "icon-import" || cmd == "icon-set-shape"
          || cmd == "icon-set-color" || cmd == "rebuild"))
    {
      if (!vmu.save (image_path))
        {
          std::cerr << "Error: failed to save image\n";
          return 1;
        }
    }

  return ret;
}
