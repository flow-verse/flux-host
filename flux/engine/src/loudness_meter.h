// SPDX-FileCopyrightText: © 2026 Flux contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace flux::engine
{

/**
 * @brief Streaming ITU-R BS.1770-4 loudness and true-peak meter.
 *
 * Integrated loudness uses K-weighting (the libebur128/pyloudnorm filter
 * formulas, valid at any sample rate), 400 ms blocks with 75% overlap, an
 * absolute gate at -70 LUFS and a relative gate 10 LU below the ungated
 * mean. Channels are weighted 1.0 (mono and stereo; no surround weights).
 *
 * True peak is the largest magnitude after 4x oversampling with a 48-tap
 * Kaiser-windowed sinc interpolator (BS.1770-4 Annex 2 asks for at least 4x
 * and a 48-tap filter; this is an equivalent design, not the reference table).
 *
 * Offline use only (allocates as it accumulates 100 ms sub-block energies).
 */
class LoudnessMeter
{
public:
  LoudnessMeter (double sample_rate, int channels)
      : channels_ (std::max (1, channels)),
        sub_len_ (std::max<long> (1, std::lround (sample_rate * 0.1))),
        state_ (static_cast<size_t> (channels_)),
        tp_hist_ (static_cast<size_t> (channels_), std::array<double, 2 * kTaps>{}),
        tp_pos_ (static_cast<size_t> (channels_), 0)
  {
    // Stage 1: high shelf (head effects)
    {
      const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
      const double K = std::tan (std::numbers::pi * f0 / sample_rate);
      const double Vh = std::pow (10.0, G / 20.0);
      const double Vb = std::pow (Vh, 0.4996667741545416);
      const double a0 = 1.0 + K / Q + K * K;
      s1_ = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0,
              (Vh - Vb * K / Q + K * K) / a0, 2.0 * (K * K - 1.0) / a0,
              (1.0 - K / Q + K * K) / a0 };
    }
    // Stage 2: high pass (RLB)
    {
      const double f0 = 38.13547087602444, Q = 0.5003270373238773;
      const double K = std::tan (std::numbers::pi * f0 / sample_rate);
      const double a0 = 1.0 + K / Q + K * K;
      s2_ = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
    }
    // 4x interpolator: h[n] = sinc((n - c) / 4) * kaiser[n], DC gain ~4 (1 per phase)
    constexpr int    L = kTaps * kPhases;
    constexpr double beta = 8.0;
    const double     c = (L - 1) / 2.0;
    const auto       i0 = [] (double x) {
      double sum = 1.0, term = 1.0;
      for (int k = 1; k < 40; ++k)
        {
          term *= (x / (2.0 * k)) * (x / (2.0 * k));
          sum += term;
        }
      return sum;
    };
    std::array<double, L> h{};
    for (int n = 0; n < L; ++n)
      {
        const double t = (n - c) / kPhases;
        const double sinc =
          t == 0.0 ? 1.0 : std::sin (std::numbers::pi * t) / (std::numbers::pi * t);
        const double r = (n - c) / c;
        h[n] = sinc * i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
      }
    for (int p = 0; p < kPhases; ++p)
      {
        double sum = 0.0;
        for (int k = 0; k < kTaps; ++k)
          sum += h[static_cast<size_t> (k * kPhases + p)];
        for (int k = 0; k < kTaps; ++k)
          phase_[p][k] = h[static_cast<size_t> (k * kPhases + p)] / sum;
      }
  }

  /** Meters @p frames frames of planar float audio. */
  void process (const float * const * data, int frames)
  {
    for (int i = 0; i < frames; ++i)
      {
        double energy = 0.0;
        for (int ch = 0; ch < channels_; ++ch)
          {
            const double x = data[ch][i];
            sample_peak_ = std::max (sample_peak_, std::abs (x));
            true_peak (static_cast<size_t> (ch), x);
            auto      &st = state_[static_cast<size_t> (ch)];
            const double y1 = s1_[0] * x + s1_[1] * st[0] + s1_[2] * st[1] - s1_[3] * st[2] - s1_[4] * st[3];
            st[1] = st[0];
            st[0] = x;
            st[3] = st[2];
            st[2] = y1;
            const double y2 = s2_[0] * y1 + s2_[1] * st[4] + s2_[2] * st[5] - s2_[3] * st[6] - s2_[4] * st[7];
            st[5] = st[4];
            st[4] = y1;
            st[7] = st[6];
            st[6] = y2;
            energy += y2 * y2;
          }
        sub_acc_ += energy;
        if (++sub_n_ == sub_len_)
          {
            subs_.push_back (sub_acc_ / static_cast<double> (sub_len_));
            sub_acc_ = 0.0;
            sub_n_ = 0;
          }
      }
  }

  /** Integrated loudness in LUFS, or nullopt below the absolute gate / under 400 ms. */
  std::optional<double> integrated_lufs () const
  {
    if (subs_.size () < 4)
      return std::nullopt;
    std::vector<double> blocks;
    blocks.reserve (subs_.size () - 3);
    for (size_t j = 0; j + 3 < subs_.size (); ++j)
      blocks.push_back ((subs_[j] + subs_[j + 1] + subs_[j + 2] + subs_[j + 3]) / 4.0);
    const auto lufs = [] (double z) { return -0.691 + 10.0 * std::log10 (z); };
    const auto gated_mean = [&] (double gate_lufs) -> std::optional<double> {
      double sum = 0.0;
      size_t n = 0;
      for (const double z : blocks)
        if (z > 0.0 && lufs (z) > gate_lufs)
          {
            sum += z;
            ++n;
          }
      if (n == 0)
        return std::nullopt;
      return sum / static_cast<double> (n);
    };
    const auto abs_mean = gated_mean (-70.0);
    if (!abs_mean)
      return std::nullopt;
    const auto rel_mean = gated_mean (lufs (*abs_mean) - 10.0);
    if (!rel_mean)
      return std::nullopt;
    return lufs (*rel_mean);
  }

  /** Largest oversampled magnitude (linear). */
  double true_peak () const { return std::max (true_peak_, sample_peak_); }
  double sample_peak () const { return sample_peak_; }

private:
  static constexpr int kPhases = 4;
  static constexpr int kTaps = 12;

  void true_peak (size_t ch, double x)
  {
    auto &buf = tp_hist_[ch];
    // Double-length ring per channel: the newest kTaps samples are contiguous
    // at buf[p..p+kTaps), newest first
    auto &p = tp_pos_[ch];
    p = (p + kTaps - 1) % kTaps;
    buf[p] = x;
    buf[p + kTaps] = x;
    for (int ph = 0; ph < kPhases; ++ph)
      {
        double y = 0.0;
        for (int k = 0; k < kTaps; ++k)
          y += phase_[ph][k] * buf[p + static_cast<size_t> (k)];
        true_peak_ = std::max (true_peak_, std::abs (y));
      }
  }

  int                                          channels_;
  long                                         sub_len_;
  std::array<double, 5>                        s1_{};
  std::array<double, 5>                        s2_{};
  std::vector<std::array<double, 8>>           state_;
  std::vector<double>                          subs_;
  double                                       sub_acc_ = 0.0;
  long                                         sub_n_ = 0;
  double                                       sample_peak_ = 0.0;
  double                                       true_peak_ = 0.0;
  std::array<std::array<double, kTaps>, kPhases> phase_{};
  std::vector<std::array<double, 2 * kTaps>>   tp_hist_;
  std::vector<size_t>                          tp_pos_;
};

} // namespace flux::engine
