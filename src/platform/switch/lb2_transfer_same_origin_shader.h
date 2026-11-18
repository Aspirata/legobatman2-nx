#pragma once
#include <cstdint>
namespace spv { class Builder; }
struct Lb2TransferSameOriginCoordinates {uint32_t x,y;};
Lb2TransferSameOriginCoordinates Lb2BuildTransferSameOriginCoordinates(
    spv::Builder&,uint32_t dest_pixel_x,uint32_t dest_pixel_y,uint32_t dest_sample_id);
