/**
 * @file session_id.h
 * @brief Lightweight session-id generator for the multi-session handler
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC
 *
 * Generates RFC 4122 v4 UUIDs (random) using only <random> + <sstream>.
 * We don't pull in libuuid for one 36-byte string per session.
 *
 * The format is exactly the canonical 8-4-4-4-12 lowercase hex, e.g.
 *   "550e8400-e29b-41d4-a716-446655440000"
 *
 * Suitable for session keys — globally unique with collision probability
 * ~10^-15 across the lifetime of any reasonable deployment.
 */
#pragma once

#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace hal {
namespace server {

inline std::string GenerateSessionId() {
    // Thread-local generator so concurrent Start* calls don't serialize
    // through a global mutex. seed_seq seeds from random_device the first
    // time; subsequent calls reuse the generator state.
    static thread_local std::mt19937_64 gen = [] {
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), rd(), rd(),
                          rd(), rd(), rd(), rd()};
        return std::mt19937_64(seq);
    }();

    std::uniform_int_distribution<uint64_t> dist;
    const uint64_t a = dist(gen);
    const uint64_t b = dist(gen);

    // Set the v4 + variant bits per RFC 4122 §4.4.
    const uint64_t a_v4 = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    const uint64_t b_v4 = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8)  << ((a_v4 >> 32) & 0xFFFFFFFFULL) << '-'
       << std::setw(4)  << ((a_v4 >> 16) & 0xFFFFULL)     << '-'
       << std::setw(4)  << ( a_v4        & 0xFFFFULL)    << '-'
       << std::setw(4)  << ((b_v4 >> 48) & 0xFFFFULL)     << '-'
       << std::setw(12) << ( b_v4        & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

} // namespace server
} // namespace hal
