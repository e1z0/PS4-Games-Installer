#pragma once
#include <cstdint>

// Explicit ABI declarations for pinned SDK entries with missing parameters.
// The HTTP declarations type the functions resolved from one module handle.
extern "C" {
int psgiHttpSetAutoRedirect(int id, int enabled) __asm__("sceHttpSetAutoRedirect");
int psgiHttpSetRecvTimeOut(int id, uint32_t microseconds) __asm__("sceHttpSetRecvTimeOut");
int psgiHttpsGetSslError(int id, int* error, uint32_t* detail) __asm__("sceHttpsGetSslError");
int psgiSslTerm(int id) __asm__("sceSslTerm");
}
