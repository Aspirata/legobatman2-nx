#pragma once
#include <cstdint>
#include <vector>

// Vulkan compute contract is documented in the stencil-copy implementation plan.
std::vector<uint32_t> Lb2BuildStencilCopyShader();
