#pragma once
#include <cstdint>
namespace spv { class Builder; }
struct Lb2DumpTileCoordinates { uint32_t x,y; };
Lb2DumpTileCoordinates Lb2BuildDumpTileCoordinates(spv::Builder&,uint32_t numerator_id,
                                                  uint32_t dynamic_pitch_id,
                                                  uint32_t specialized_pitch);
