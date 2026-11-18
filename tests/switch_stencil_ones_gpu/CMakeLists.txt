cmake_minimum_required(VERSION 3.25)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
project(stencil_ones_gpu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(sdk "${root}/third_party/nfsmw-nx/sdk")
set(spv "${sdk}/thirdparty/glslang/SPIRV")
add_executable(stencil-ones-gpu main.cpp "${root}/src/platform/switch/stencil_copy_shader.cpp" "${root}/src/platform/switch/stencil_ones_shader.cpp"
 "${spv}/SpvBuilder.cpp" "${spv}/SpvPostProcess.cpp" "${spv}/InReadableOrder.cpp" "${spv}/Logger.cpp")
target_include_directories(stencil-ones-gpu PRIVATE "${root}/src/platform/switch" "${sdk}/thirdparty/glslang" "${sdk}/thirdparty/vulkan-headers/include")

