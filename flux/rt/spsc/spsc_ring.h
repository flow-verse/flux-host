// Lock-free single-producer/single-consumer ring buffer for float audio frames.
#pragma once
#include <atomic>
#include <cstddef>
#include <vector>

namespace flux::rt {

class SpscRing {
 public:
  explicit SpscRing(size_t capacity_pow2) : buf_(capacity_pow2), mask_(capacity_pow2 - 1) {}

  // Producer thread only. Returns number of samples written.
  size_t push(const float* src, size_t n) {
    const size_t w = w_.load(std::memory_order_relaxed);
    const size_t r = r_.load(std::memory_order_acquire);
    const size_t free = buf_.size() - (w - r);
    if (n > free) n = free;
    for (size_t i = 0; i < n; ++i) buf_[(w + i) & mask_] = src[i];
    w_.store(w + n, std::memory_order_release);
    return n;
  }

  // Consumer (audio) thread only. Never blocks; returns samples read (may be < n).
  size_t pop(float* dst, size_t n) {
    const size_t r = r_.load(std::memory_order_relaxed);
    const size_t w = w_.load(std::memory_order_acquire);
    const size_t avail = w - r;
    if (n > avail) n = avail;
    for (size_t i = 0; i < n; ++i) dst[i] = buf_[(r + i) & mask_];
    r_.store(r + n, std::memory_order_release);
    return n;
  }

 private:
  std::vector<float> buf_;
  const size_t mask_;
  alignas(64) std::atomic<size_t> w_{0};
  alignas(64) std::atomic<size_t> r_{0};
};

}  // namespace flux::rt
