/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "BLO_read_write.hh"

#include "BKE_image_format.hh"

#include "RNA_prototypes.hh"

#include "NOD_compositor_file_output.hh"

namespace blender::nodes {

StructRNA **FileOutputItemsAccessor::item_srna = &RNA_NodeCompositorFileOutputItem;

void FileOutputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  writer->write_string(item.name);
  BKE_image_format_blend_write(writer, const_cast<ImageFormatData *>(&item.format));
}

void FileOutputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
  BKE_image_format_blend_read_data(reader, &item.format);
}

std::string FileOutputItemsAccessor::validate_name(const StringRef name)
{
  char file_name[FILE_MAX] = "";
  STRNCPY(file_name, name.data());
  BLI_path_make_safe(file_name);
  return file_name;
}

}  // namespace blender::nodes
