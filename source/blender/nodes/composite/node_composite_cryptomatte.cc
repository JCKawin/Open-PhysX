/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string>

#include "BLI_listbase.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.hh"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "BKE_cryptomatte.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_node_legacy_types.hh"

#include "IMB_imbuf_types.hh"

#include "MEM_guardedalloc.h"

#include "NOD_composite.hh"

namespace blender {

static bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node_render(
    const bNode &node, const bool build_meta_data)
{
  bke::cryptomatte::CryptomatteSessionPtr session;

  Scene *scene = id_cast<Scene *>(node.id);
  if (!scene) {
    return session;
  }
  BLI_assert(GS(scene->id.name) == ID_SCE);

  session = bke::cryptomatte::CryptomatteSessionPtr(
      BKE_cryptomatte_init_from_scene(scene, build_meta_data));
  return session;
}

static bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node_image(const bNode &node)
{
  bke::cryptomatte::CryptomatteSessionPtr session;
  Image *image = id_cast<Image *>(node.id);
  if (!image) {
    return session;
  }
  BLI_assert(GS(image->id.name) == ID_IM);

  NodeCryptomatte *node_cryptomatte = static_cast<NodeCryptomatte *>(node.storage);
  ImageUser image_user = node_cryptomatte->iuser;
  BKE_image_user_frame_calc(image, &image_user, image_user.framenr);

  if (!(image_user.flag & IMA_USER_FRAME_IN_RANGE)) {
    image_user.framenr = BKE_image_sequence_guess_offset(image);
  }

  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &image_user, nullptr);
  RenderResult *render_result = image->rr;
  if (render_result) {
    session = bke::cryptomatte::CryptomatteSessionPtr(
        BKE_cryptomatte_init_from_render_result(render_result));
  }
  BKE_image_release_ibuf(image, ibuf, nullptr);
  return session;
}

static bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node(
    const bNode &node, const bool build_meta_data)
{
  bke::cryptomatte::CryptomatteSessionPtr session;
  if (node.type_legacy != CMP_NODE_CRYPTOMATTE) {
    return session;
  }

  switch (node.custom1) {
    case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER: {
      return cryptomatte_init_from_node_render(node, build_meta_data);
    }

    case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE: {
      return cryptomatte_init_from_node_image(node);
    }
  }
  return session;
}

static CryptomatteEntry *cryptomatte_find(const NodeCryptomatte &n, float encoded_hash)
{
  for (CryptomatteEntry &entry : n.entries) {
    if (entry.encoded_hash == encoded_hash) {
      return &entry;
    }
  }
  return nullptr;
}

static void cryptomatte_add(bNode &node, NodeCryptomatte &node_cryptomatte, float encoded_hash)
{
  if (cryptomatte_find(node_cryptomatte, encoded_hash)) {
    return;
  }

  CryptomatteEntry *entry = MEM_new<CryptomatteEntry>(__func__);
  entry->encoded_hash = encoded_hash;
  bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(node, true);
  if (session) {
    BKE_cryptomatte_find_name(session.get(), encoded_hash, entry->name, sizeof(entry->name));
  }

  BLI_addtail(&node_cryptomatte.entries, entry);
}

static void cryptomatte_remove(NodeCryptomatte &n, float encoded_hash)
{
  CryptomatteEntry *entry = cryptomatte_find(n, encoded_hash);
  if (!entry) {
    return;
  }
  BLI_remlink(&n.entries, entry);
  MEM_delete(entry);
}

void ntreeCompositCryptomatteSyncFromAdd(bNode *node)
{
  BLI_assert(ELEM(node->type_legacy, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.add[0] != 0.0f) {
    cryptomatte_add(*node, *n, n->runtime.add[0]);
    zero_v3(n->runtime.add);
  }
}

void ntreeCompositCryptomatteSyncFromRemove(bNode *node)
{
  BLI_assert(ELEM(node->type_legacy, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.remove[0] != 0.0f) {
    cryptomatte_remove(*n, n->runtime.remove[0]);
    zero_v3(n->runtime.remove);
  }
}

void ntreeCompositCryptomatteUpdateLayerNames(bNode *node)
{
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  n->runtime.layers.free_no_destruct();

  bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(*node, false);

  if (session) {
    for (StringRef layer_name : bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session)) {
      CryptomatteLayer *layer = MEM_new<CryptomatteLayer>(__func__);
      layer_name.copy_utf8_truncated(layer->name);
      BLI_addtail(&n->runtime.layers, layer);
    }
  }
}

void ntreeCompositCryptomatteLayerPrefix(const bNode *node, char *r_prefix, size_t prefix_maxncpy)
{
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *node_cryptomatte = static_cast<NodeCryptomatte *>(node->storage);
  bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(*node, false);
  std::string first_layer_name;

  if (session) {
    for (StringRef layer_name : bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session)) {
      if (first_layer_name.empty()) {
        first_layer_name = layer_name;
      }

      if (layer_name == node_cryptomatte->layer_name) {
        BLI_strncpy_utf8(r_prefix, node_cryptomatte->layer_name, prefix_maxncpy);
        return;
      }
    }
  }

  const char *cstr = first_layer_name.c_str();
  BLI_strncpy_utf8(r_prefix, cstr, prefix_maxncpy);
}

CryptomatteSession *ntreeCompositCryptomatteSession(bNode *node)
{
  bke::cryptomatte::CryptomatteSessionPtr session_ptr = cryptomatte_init_from_node(*node, true);
  return session_ptr.release();
}

void ntreeCompositCryptomatteAddSocket(bNode *node)
{
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  n->inputs_num++;
}

bool ntreeCompositCryptomatteRemoveSocket(bNode *node)
{
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->inputs_num < 2) {
    return false;
  }
  n->inputs_num--;
  return true;
}

}  // namespace blender
