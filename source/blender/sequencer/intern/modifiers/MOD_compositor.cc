/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLT_translation.hh"

#include "DNA_node_types.h"
#include "DNA_sequence_types.h"

#include "BKE_idprop.hh"

#include "DEG_depsgraph.hh"

#include "NOD_compositor_nodes_caller_ui.hh"

#include "SEQ_modifier.hh"

#include "UI_interface.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "modifier.hh"

namespace blender::seq {

void compositor_modifier_nodes_update_interface(Main &bmain,
                                                Scene &sequencer_scene,
                                                SequencerCompositorModifierData &cmd)
{
  if (!cmd.modifier.system_properties) {
    cmd.modifier.system_properties =
        bke::idprop::create_group("SequencerCompositorModifierProperties").release();
  }

  if (!cmd.node_group || ID_MISSING(cmd.node_group)) {
    return;
  }

  PointerRNA properties_ptr = RNA_pointer_create_discrete(
      &sequencer_scene.id, RNA_SequencerCompositorModifierProperties, &cmd);
  RNA_ensure_and_sync_system_properties(bmain, properties_ptr, *cmd.modifier.system_properties);

  DEG_id_tag_update(&sequencer_scene.id, ID_RECALC_SEQUENCER_STRIPS);
}

static void compositor_modifier_init_data(StripModifierData *strip_modifier_data)
{
  SequencerCompositorModifierData *modifier_data =
      reinterpret_cast<SequencerCompositorModifierData *>(strip_modifier_data);
  modifier_data->node_group = nullptr;
}

static void compositor_modifier_apply(ModifierApplyContext & /*context*/,
                                      StripModifierData * /*strip_modifier_data*/)
{
}

static PointerRNA *modifier_panel_get_property_pointers(Panel *panel)
{
  PointerRNA *ptr = ui::panel_custom_data_get(panel);
  BLI_assert(*ptr);
  BLI_assert(RNA_struct_is_a(ptr->type, RNA_StripModifier));
  ui::panel_context_pointer_set(panel, "modifier", ptr);
  return ptr;
}

static void compositor_modifier_panel_draw(const bContext *C, Panel *panel)
{
  ui::Layout &layout = *panel->layout;
  PointerRNA *modifier_ptr = modifier_panel_get_property_pointers(panel);
  nodes::draw_compositor_nodes_modifier_ui(*C, modifier_ptr, layout);
}

static void compositor_modifier_register(ARegionType *region_type)
{
  modifier_panel_register(
      region_type, eSeqModifierType_Compositor, compositor_modifier_panel_draw);
}

StripModifierTypeInfo seqModifierType_Compositor = {
    /*idname*/ "Compositor",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Compositor"),
    /*struct_name*/ "SequencerCompositorModifierData",
    /*struct_size*/ sizeof(SequencerCompositorModifierData),
    /*init_data*/ compositor_modifier_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ compositor_modifier_apply,
    /*panel_register*/ compositor_modifier_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};

}  // namespace blender::seq
