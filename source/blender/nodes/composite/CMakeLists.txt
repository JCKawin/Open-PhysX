# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  include
  ../intern
  ../../editors/include
  ../../makesrna
  ../../sequencer

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  node_composite_cryptomatte.cc
  node_composite_file_output_items.cc
  node_composite_tree.cc
  node_composite_util.cc

  include/NOD_compositor_file_output.hh
  node_composite_util.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::blenloader
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::render
  PRIVATE bf::windowmanager
  PRIVATE bf::sequencer
  PRIVATE bf::dependencies::opencolorio
  PRIVATE bf::dependencies::optional::tbb
  PRIVATE bf::dependencies::optional::openimagedenoise
  PRIVATE bf::dependencies::optional::fftw3
)

add_node_discovery(
  bf_nodes_compositor_generated
  "${SRC}"
  ${CMAKE_CURRENT_BINARY_DIR}/register_compositor_nodes.cc
  register_compositor_nodes
)

list(APPEND LIB
  bf_nodes_compositor_generated
)

blender_add_lib(bf_nodes_composite "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
target_link_libraries(bf_nodes_compositor_generated bf_nodes_composite)

blender_set_target_unity_build(bf_nodes_composite 10)

# RNA_prototypes.hh
add_dependencies(bf_nodes_composite bf_rna)
