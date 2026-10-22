# SPSC ring buffer
Single-producer / single-consumer lock-free queue (power-of-two capacity, acquire/release atomics, cache-line-padded indices).
Producer: AI inference thread. Consumer: audio callback. Consumer must never block; on empty it outputs silence or dry signal.
