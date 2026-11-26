/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#pragma once

namespace blender::seq {

/* Placeholder owned by EditingRuntime. The compositor engine no longer fills it. */
class CompositorCache {
 public:
  CompositorCache() = default;
  ~CompositorCache() = default;
};

}  // namespace blender::seq
