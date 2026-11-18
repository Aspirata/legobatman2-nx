cmake_minimum_required(VERSION 3.25)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
project(memexport_spirv LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(sdk "${root}/third_party/nfsmw-nx/sdk")
set(shader "${sdk}/src/graphics/pipeline/shader")
set(spv "${sdk}/thirdparty/glslang/SPIRV")
file(READ "${shader}/spirv_translator_memexport.cpp" memexport)
# Enable just this translation unit's Switch optimization on the host. The
# platform headers and all other sources retain their native Windows settings.
string(REPLACE "#if REX_PLATFORM_SWITCH" "#if 1" memexport "${memexport}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/memexport.cpp" "${memexport}")
file(READ "${shader}/spirv_translator_rb.cpp" rb)
string(REPLACE "#if REX_PLATFORM_SWITCH" "#if 1" rb "${rb}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rb.cpp" "${rb}")
add_executable(fsi-spirv main.cpp
 "${shader}/shader.cpp" "${shader}/translator.cpp" "${shader}/translator_disasm.cpp"
 "${sdk}/src/graphics/format/ucode.cpp"
 "${sdk}/src/graphics/util/draw.cpp"
 "${sdk}/src/graphics/registers.cpp"
 "${shader}/spirv.cpp" "${shader}/spirv_builder.cpp" "${shader}/spirv_translator.cpp"
 "${shader}/spirv_translator_alu.cpp" "${shader}/spirv_translator_fetch.cpp"
 "${CMAKE_CURRENT_BINARY_DIR}/rb.cpp" "${CMAKE_CURRENT_BINARY_DIR}/memexport.cpp"
 "${spv}/SpvBuilder.cpp" "${spv}/SpvPostProcess.cpp" "${spv}/InReadableOrder.cpp"
 "${spv}/Logger.cpp")
target_include_directories(fsi-spirv PRIVATE "${sdk}/include"
 "${sdk}/thirdparty/glslang" "${sdk}/thirdparty/vulkan-headers/include"
 "${root}/src/platform/switch")
target_link_libraries(fsi-spirv PRIVATE rex::runtime rex::xxhash rex::renderdoc)
target_compile_options(fsi-spirv PRIVATE /clang:-msse4.1 /EHa)
