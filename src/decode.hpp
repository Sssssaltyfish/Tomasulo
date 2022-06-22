#pragma once

#include "defines.hpp"
#include <string.h>
#include <type_traits>

// `from` shall be greater than `to`
inline __attribute__((always_inline)) constexpr word getField(word w, word from, word to) {
    return (w >> to) & ((1 << (from - to + 1)) - 1);
}

inline constexpr word maskN(word N) {
    return (1 << N) - 1;
}

// convert an N bit number into a 32-bit or 64-bit number
template <const word N> inline constexpr word signExtend(word num) {
    constexpr word shift = sizeof(word) * 8 - N;
    return (std::make_signed_t<word>(num) << shift) >> shift;
}
inline constexpr word opcode(word instr) {
    return getField(instr, 31, 26);
}

inline constexpr word reg1(word instr) {
    return getField(instr, 25, 21);
}

inline constexpr word reg2(word instr) {
    return getField(instr, 20, 16);
}

inline constexpr word reg3(word instr) {
    return getField(instr, 15, 11);
}

inline constexpr word func(word instr) {
    return getField(instr, 10, 0);
}

inline constexpr word immEx(word instr) {
    return signExtend<16>(getField(instr, 15, 0));
}

inline constexpr word jmpOffsetEx(word instr) {
    return signExtend<26>(getField(instr, 25, 0));
}
