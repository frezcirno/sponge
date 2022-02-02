#include "wrapping_integers.hh"

using namespace std;

constexpr uint64_t WRAP = 1UL << 32;
constexpr uint64_t HALF_WRAP = 1UL << 31;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return WrappingInt32{static_cast<uint32_t>(n + isn.raw_value())}; }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t m = n.raw_value() - isn.raw_value();
    uint64_t x = (checkpoint & 0xFFFFFFFF00000000ULL) | m;
    if (x + HALF_WRAP < checkpoint)
        return x + WRAP;
    if (x < WRAP || x < checkpoint + HALF_WRAP)
        return x;
    return x - WRAP;
}
