#pragma once

// The pinned Windows generator emits GCC aliases before extern "C". GCC
// ignores them there, leaving the guest dispatch entry undefined. Keep both
// the overridable weak name and the original entry by attaching attributes
// to the declarator, as in the pinned Horizon generator template.
#if defined(__SWITCH__) && defined(__GNUC__) && !defined(__clang__)
#ifndef DEFINE_REX_FUNC
#define DEFINE_REX_FUNC(name) \
  extern "C" REX_FUNC(name) __attribute__((alias("__imp__" #name), weak, noinline)); \
  REX_EXTERN(__imp__##name)
#endif
#endif
