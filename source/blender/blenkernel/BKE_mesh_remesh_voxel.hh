/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

namespace blender {

struct Mesh;
struct ModifierData;
struct Object;
struct ReportList;

Mesh *BKE_mesh_remesh_voxel_fix_poles(const Mesh *mesh);
Mesh *BKE_mesh_remesh_voxel(const Mesh *mesh,
                            float voxel_size,
                            float adaptivity,
                            float isovalue,
                            const Object *object,
                            ModifierData *modifier_data);
Mesh *BKE_mesh_remesh_voxel(
    const Mesh *mesh, float voxel_size, float adaptivity, float isovalue, ReportList *reports);

namespace bke {
void mesh_remesh_reproject_attributes(const Mesh &src, Mesh &dst);
}

}  // namespace blender
