cmake_minimum_required(VERSION 3.25)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
project(transfer_same_origin_gpu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(sdk "${root}/third_party/nfsmw-nx/sdk")
set(spv "${sdk}/thirdparty/glslang/SPIRV")
add_executable(transfer-same-origin-gpu main.cpp "${root}/src/platform/switch/transfer_same_origin_shader.cpp"
 "${spv}/SpvBuilder.cpp" "${spv}/SpvPostProcess.cpp" "${spv}/InReadableOrder.cpp" "${spv}/Logger.cpp")
target_include_directories(transfer-same-origin-gpu PRIVATE "${root}/src/platform/switch" "${sdk}/include" "${root}/work/toolchain/rexglue-0.10.0/win-amd64/include" "${sdk}/src/graphics/shaders/vulkan_spirv" "${sdk}/thirdparty/glslang" "${sdk}/thirdparty/vulkan-headers/include")




