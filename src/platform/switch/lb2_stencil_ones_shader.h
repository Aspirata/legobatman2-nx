#pragma once
#include "lb2_stencil_ones.h"
namespace spv { class Builder; }
uint32_t Lb2BuildStencilKill(spv::Builder&,uint32_t packed_id,uint32_t mask_id,
                            lb2::stencil_ones::ShaderMode);
