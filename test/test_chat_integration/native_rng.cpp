// Firmware identity generation uses its injected RNG. Library hardware entropy
// must never be used in this deterministic native integration harness.
#include <RNG.h>
#include <stdlib.h>

RNGClass::RNGClass() {}
RNGClass::~RNGClass() {}
void RNGClass::rand(uint8_t*, size_t) { abort(); }
RNGClass RNG;
