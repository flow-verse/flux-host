// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <cstdint>

namespace zrythm_test_plugins
{

/** Shared DSP for the Test Gain fixtures (VST3 and CLAP). */
inline void
apply_gain (const float * in, float * out, uint32_t num_samples, double gain)
{
  const auto g = static_cast<float> (gain);
  for (uint32_t n = 0; n < num_samples; ++n)
    out[n] = in[n] * g;
}

} // namespace zrythm_test_plugins
