/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_resource_scope.hh"

#include "BKE_idprop.hh"
#include "BKE_node_runtime.hh"

#include "DEG_depsgraph.hh"

#include "DNA_ID.h"
#include "DNA_sequence_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "NOD_socket_usage_inference.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "SEQ_effects.hh"
#include "SEQ_render.hh"

#include "effects.hh"

namespace blender::seq {

static SeqResult do_compositor_effect(const RenderData *context,
                                      SeqRenderState * /*state*/,
                                      Strip * /*strip*/,
                                      float /*timeline_frame*/,
                                      float /*fac*/,
                                      const SeqResult & /*src1*/,
                                      const SeqResult & /*src2*/)
{
  const int x = context->rectx;
  const int y = context->recty;
  SeqResult out;
  out.image = IMB_allocImBuf(x, y, ImBufFlags::FloatData | ImBufFlags::UninitializedPixels);
  IMB_colormanagement_assign_float_colorspace(
      out.image, IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR));
  IMB_rectfill(out.image, float4(0, 0, 0, 1));
  out.image->color_mode = ImColorMode::RGB;
  out.is_opaque_before_transform = true;
  return out;
}

static void init_compositor_effect(Strip *strip)
{
  CompositorEffectVars *data = MEM_new<CompositorEffectVars>(__func__);
  strip->effectdata = data;
}

static void free_compositor_effect(Strip *strip, const bool /*do_id_user*/)
{
  if (strip->effectdata) {
    CompositorEffectVars *data = static_cast<CompositorEffectVars *>(strip->effectdata);
    if (data->system_properties != nullptr) {
      IDP_FreeProperty_ex(data->system_properties, false);
    }
    MEM_delete(data);
    strip->effectdata = nullptr;
  }
}

static void copy_compositor_effect(Strip *dst, const Strip *src, const int flag)
{
  CompositorEffectVars *dst_data = MEM_new<CompositorEffectVars>(__func__);
  const auto *src_data = static_cast<const CompositorEffectVars *>(src->effectdata);
  *dst_data = *src_data;
  dst_data->system_properties = nullptr;
  if (src_data->system_properties != nullptr) {
    dst_data->system_properties = IDP_CopyProperty_ex(src_data->system_properties, flag);
  }
  dst->effectdata = dst_data;
}

static StripEarlyOut early_out_compositor(const Strip *strip, float /*fac*/)
{
  if (strip->input1 == nullptr) {
    return StripEarlyOut::NoInput;
  }
  return StripEarlyOut::DoEffect;
}

void compositor_effect_nodes_update_interface(Main &bmain, Scene &sequencer_scene, Strip &strip)
{
  if (strip.type != STRIP_TYPE_COMPOSITOR || strip.effectdata == nullptr) {
    return;
  }
  CompositorEffectVars *comp = static_cast<CompositorEffectVars *>(strip.effectdata);
  if (!comp->system_properties) {
    comp->system_properties =
        bke::idprop::create_group("SequencerCompositorEffectProperties").release();
  }

  if (!comp->node_group || ID_MISSING(comp->node_group)) {
    return;
  }

  PointerRNA properties_ptr = RNA_pointer_create_discrete(
      &sequencer_scene.id, RNA_SequencerCompositorEffectProperties, comp);
  RNA_ensure_and_sync_system_properties(bmain, properties_ptr, *comp->system_properties);

  DEG_id_tag_update(&sequencer_scene.id, ID_RECALC_SEQUENCER_STRIPS);
}

void compositor_effect_nodes_input_usages(const Scene &sequencer_scene,
                                          Strip &strip,
                                          Vector<bool> &r_used,
                                          Vector<bool> &r_visible)
{
  r_used.clear();
  r_visible.clear();
  if (strip.type != STRIP_TYPE_COMPOSITOR || strip.effectdata == nullptr) {
    return;
  }
  CompositorEffectVars *comp = static_cast<CompositorEffectVars *>(strip.effectdata);
  if (comp->node_group == nullptr || ID_MISSING(comp->node_group)) {
    return;
  }
  bNodeTree &tree = *comp->node_group;
  tree.ensure_interface_cache();
  const int inputs_num = tree.interface_inputs().size();
  if (inputs_num == 0) {
    return;
  }

  PointerRNA strip_ptr = RNA_pointer_create_discrete(
      &const_cast<Scene &>(sequencer_scene).id, RNA_CompositorStrip, &strip);
  PointerRNA properties_ptr = RNA_pointer_get(&strip_ptr, "properties");
  if (!properties_ptr) {
    return;
  }

  ResourceScope scope;
  Vector<nodes::InferenceValue> input_values = nodes::get_geometry_nodes_input_inference_values(
      tree, properties_ptr, scope);

  const int images_num = strip.effect_num_inputs_get();
  int float_counter = 0;
  int color_counter = 0;
  for (const int i : IndexRange(inputs_num)) {
    const bNodeTreeInterfaceSocket &socket = *tree.interface_inputs()[i];
    const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (socket_type == SOCK_FLOAT && float_counter == 0) {
      input_values[i] = nodes::InferenceValue::Unknown();
      float_counter++;
    }
    else if (socket_type == SOCK_RGBA && color_counter < images_num) {
      input_values[i] = nodes::InferenceValue::Unknown();
      color_counter++;
    }
  }

  Array<nodes::socket_usage_inference::SocketUsage> usages(inputs_num);
  nodes::socket_usage_inference::infer_group_interface_usage(tree, input_values, usages);
  r_used.resize(inputs_num);
  r_visible.resize(inputs_num);
  for (const int i : IndexRange(inputs_num)) {
    r_used[i] = usages[i].is_used;
    r_visible[i] = usages[i].is_visible;
  }
}

void compositor_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_compositor_effect;
  rval.free = free_compositor_effect;
  rval.copy = copy_compositor_effect;
  rval.execute = do_compositor_effect;
  rval.early_out = early_out_compositor;
}

}  // namespace blender::seq
