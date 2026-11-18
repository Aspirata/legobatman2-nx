#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define VK_NO_PROTOTYPES
#include "lb2_direct_color_shader.h"
#include <rex/graphics/util/draw.h>
#include "resolve_fast_32bpp_1x2xmsaa_cs.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#define FUNCS(X)                                                                             \
  X(CreateInstance)                                                                          \
  X(EnumeratePhysicalDevices)                                                                \
  X(GetPhysicalDeviceProperties)                                                             \
  X(GetPhysicalDeviceFeatures)                                                               \
  X(GetPhysicalDeviceQueueFamilyProperties) X(GetPhysicalDeviceMemoryProperties) X(          \
      GetPhysicalDeviceFormatProperties) X(GetPhysicalDeviceImageFormatProperties)           \
      X(CreateDevice) X(GetDeviceQueue) X(CreateCommandPool) X(                              \
          AllocateCommandBuffers) X(ResetCommandBuffer) X(BeginCommandBuffer) X(EndCommandBuffer)                  \
          X(QueueSubmit) X(QueueWaitIdle) X(DeviceWaitIdle) X(CreateBuffer) X(               \
              GetBufferMemoryRequirements) X(AllocateMemory) X(BindBufferMemory)             \
              X(MapMemory) X(CreateImage) X(GetImageMemoryRequirements) X(                   \
                  BindImageMemory) X(CreateImageView) X(CreateRenderPass)                    \
                  X(CreateFramebuffer) X(CreateShaderModule) X(                              \
                      CreatePipelineLayout) X(CreateGraphicsPipelines)                       \
                      X(CreateComputePipelines) X(CreateDescriptorSetLayout) X(              \
                          CreateDescriptorPool) X(AllocateDescriptorSets)                    \
                          X(UpdateDescriptorSets) X(CmdPipelineBarrier) X(                   \
                              CmdBeginRenderPass) X(CmdEndRenderPass)                        \
                              X(CmdBindPipeline) X(CmdBindVertexBuffers) X(                  \
                                  CmdSetScissor) X(CmdSetStencilReference) X(CmdDraw)        \
                                  X(CmdBindDescriptorSets) X(CmdPushConstants) X(            \
                                      CmdDispatch) X(CmdClearDepthStencilImage)              \
                                      X(CmdCopyBufferToImage) X(                             \
                                          CmdCopyImageToBuffer) X(UnmapMemory)               \
                                          X(FreeMemory) X(DestroyBuffer) X(                  \
                                              DestroyImage) X(DestroyImageView)              \
                                              X(DestroyRenderPass) X(                        \
                                                  DestroyFramebuffer) X(DestroyShaderModule) \
                                                  X(DestroyPipeline) X(                      \
                                                      DestroyPipelineLayout)                 \
                                                      X(DestroyDescriptorPool) X(            \
                                                          DestroyDescriptorSetLayout)        \
                                                          X(DestroyCommandPool) X(           \
                                                              DestroyDevice)                 \
                                                              X(DestroyInstance)
#define DECL(n) PFN_vk##n vk##n;
FUNCS(DECL)
#undef DECL
static void check(VkResult r, const char *op) {
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string(op) +
                             " VkResult=" + std::to_string(r));
}
#define VK(x) check((x), #x)
constexpr uint32_t SW = 20, SH = 12, DW = 40, DH = 24;
static uint8_t pattern(uint32_t x, uint32_t y, uint32_t s) {
  return uint8_t(x * 37 + y * 71 + s * 53 + 19);
}
struct Buffer {
  VkBuffer b;
  VkDeviceMemory m;
  uint8_t *p;
  VkDeviceSize size;
};
struct Image {
  VkImage i;
  VkDeviceMemory m;
  VkImageView view;
};
struct Harness {
  VkInstance instance{};
  VkPhysicalDevice gpu{};
  VkDevice device{};
  VkQueue queue{};
  uint32_t family{};
  VkCommandPool pool{};
  VkCommandBuffer cmd{};
  VkPhysicalDeviceMemoryProperties mem{};
  VkFormat format{};
  uint32_t memory(uint32_t bits, VkMemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
      if ((bits & (1u << i)) &&
          (mem.memoryTypes[i].propertyFlags & flags) == flags)
        return i;
    throw std::runtime_error("Required memory type unavailable");
  }
  Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    Buffer r{};
    r.size = size;
    VkBufferCreateInfo c{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    c.size = size;
    c.usage = usage;
    c.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK(vkCreateBuffer(device, &c, nullptr, &r.b));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, r.b, &req);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    a.allocationSize = req.size;
    a.memoryTypeIndex =
        memory(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK(vkAllocateMemory(device, &a, nullptr, &r.m));
    VK(vkBindBufferMemory(device, r.b, r.m, 0));
    VK(vkMapMemory(device, r.m, 0, VK_WHOLE_SIZE, 0, (void **)&r.p));
    return r;
  }
  Image image(uint32_t w, uint32_t h, VkSampleCountFlagBits samples,
              VkImageUsageFlags usage) {
    Image r{};
    VkImageCreateInfo c{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    c.imageType = VK_IMAGE_TYPE_2D;
    c.format = format;
    c.extent = {w, h, 1};
    c.mipLevels = 1;
    c.arrayLayers = 1;
    c.samples = samples;
    c.tiling = VK_IMAGE_TILING_OPTIMAL;
    c.usage = usage;
    c.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK(vkCreateImage(device, &c, nullptr, &r.i));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, r.i, &req);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    a.allocationSize = req.size;
    a.memoryTypeIndex =
        memory(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK(vkAllocateMemory(device, &a, nullptr, &r.m));
    VK(vkBindImageMemory(device, r.i, r.m, 0));
    return r;
  }
  VkImageView view(Image im, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo c{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    c.image = im.i;
    c.viewType = VK_IMAGE_VIEW_TYPE_2D;
    c.format = format;
    c.subresourceRange = {aspect, 0, 1, 0, 1};
    VkImageView v;
    VK(vkCreateImageView(device, &c, nullptr, &v));
    return v;
  }
  VkShaderModule shader(const std::vector<uint32_t> &words) {
    VkShaderModuleCreateInfo c{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    c.codeSize = words.size() * 4;
    c.pCode = words.data();
    VkShaderModule m;
    VK(vkCreateShaderModule(device, &c, nullptr, &m));
    return m;
  }
  void begin() {
    VK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK(vkBeginCommandBuffer(cmd, &b));
  }
  void submit() {
    VK(vkEndCommandBuffer(cmd));
    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    s.commandBufferCount = 1;
    s.pCommandBuffers = &cmd;
    VK(vkQueueSubmit(queue, 1, &s, VK_NULL_HANDLE));
    VK(vkQueueWaitIdle(queue));
  }
  void barrier(Image im, VkImageLayout oldLayout, VkImageLayout newLayout,
               VkAccessFlags src, VkAccessFlags dst, VkPipelineStageFlags from,
               VkPipelineStageFlags to) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = src;
    b.dstAccessMask = dst;
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = im.i;
    b.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, from, to, 0, 0, nullptr, 0, nullptr, 1, &b);
  }
  void bufferBarrier(Buffer b, VkAccessFlags src, VkAccessFlags dst,
                     VkPipelineStageFlags from, VkPipelineStageFlags to) {
    VkBufferMemoryBarrier x{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    x.srcAccessMask = src;
    x.dstAccessMask = dst;
    x.srcQueueFamilyIndex = x.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    x.buffer = b.b;
    x.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, from, to, 0, 0, nullptr, 1, &x, 0, nullptr);
  }
  bool init(VkFormat requested) {
    HMODULE dll = LoadLibraryW(L"C:\\Windows\\System32\\vulkan-1.dll");
    if (!dll)
      throw std::runtime_error("Vulkan loader unavailable");
#define LOAD(n)                                                                \
  vk##n = (PFN_vk##n)GetProcAddress(dll, "vk" #n);                             \
  if (!vk##n)                                                                  \
    throw std::runtime_error("Missing vk" #n);
    FUNCS(LOAD)
#undef LOAD
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "LB2 direct color GPU validation";
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    VK(vkCreateInstance(&ci, nullptr, &instance));
    uint32_t count = 0;
    VK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    if (!count)
      throw std::runtime_error("No Vulkan device");
    std::vector<VkPhysicalDevice> devices(count);
    VK(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
    for (auto candidate : devices) {
      uint32_t n = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, nullptr);
      std::vector<VkQueueFamilyProperties> qs(n);
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, qs.data());
      for (uint32_t j = 0; j < n; ++j)
        if ((qs[j].queueFlags &
             (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
          gpu = candidate;
          family = j;
          break;
        }
      if (gpu)
        break;
    }
    if (!gpu)
      throw std::runtime_error("No graphics+compute queue");
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpu, &props);
    printf("device=%s api=%u.%u.%u\n", props.deviceName,
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion));
    if (props.apiVersion < VK_API_VERSION_1_2)
      throw std::runtime_error("Vulkan 1.2 required");
    format = requested;
    VkImageFormatProperties ip{};
    VK(vkGetPhysicalDeviceImageFormatProperties(gpu, format, VK_IMAGE_TYPE_2D,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ip));
    float priority = 1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    VK(vkCreateDevice(gpu, &di, nullptr, &device));
    vkGetDeviceQueue(device, family, 0, &queue);
    vkGetPhysicalDeviceMemoryProperties(gpu, &mem);
    VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pc.queueFamilyIndex = family;
    VK(vkCreateCommandPool(device, &pc, nullptr, &pool));
    VkCommandBufferAllocateInfo ca{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ca.commandPool = pool;
    ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ca.commandBufferCount = 1;
    VK(vkAllocateCommandBuffers(device, &ca, &cmd));
    return true;
  }
};
// Exact address calculation from SDK texture_util::GetTiledOffset2D, util.cpp.
static uint32_t tiled(int32_t x, int32_t y, uint32_t pitch) {
  constexpr uint32_t log2 = 2;
  pitch = (pitch + 31) & ~31u;
  int32_t macro = ((x >> 5) + (y >> 5) * int32_t(pitch >> 5)) << (log2 + 7);
  int32_t micro = ((x & 7) + ((y & 0xE) << 2)) << log2;
  int32_t offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4);
  return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) +
         (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F);
}
static uint32_t pixel(uint32_t x, uint32_t y) {
  return uint32_t(pattern(x,y,0)) | (uint32_t(pattern(x,y,1))<<8) |
         (uint32_t(pattern(x,y,2))<<16) | (uint32_t(pattern(x,y,3))<<24);
}
static uint32_t convert(uint32_t v, unsigned endian, bool swap) {
  if (swap) v = (v & 0xff00ff00u) | ((v & 255u) << 16) | ((v >> 16) & 255u);
  if (endian == 1) v = ((v & 0x00ff00ffu)<<8) | ((v & 0xff00ff00u)>>8);
  if (endian == 2) v = _byteswap_ulong(v);
  if (endian == 3) v = (v << 16) | (v >> 16);
  return v;
}
struct Pipeline {
  VkDescriptorSetLayout sets[2]{}; VkPipelineLayout layout{};
  VkDescriptorPool pool{}; VkDescriptorSet ds[2]{}; VkPipeline pipeline{}; VkShaderModule module{};
};
static Pipeline makePipeline(Harness& h, const std::vector<uint32_t>& code, bool image, uint32_t pushSize,
                             Image source, Buffer input, Buffer output) {
  Pipeline p{};
  VkDescriptorSetLayoutBinding binding{}; binding.binding=0; binding.descriptorCount=1;
  binding.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
  VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  li.bindingCount=1; li.pBindings=&binding;
  binding.descriptorType=image?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  VK(vkCreateDescriptorSetLayout(h.device,&li,nullptr,&p.sets[0]));
  binding.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  VK(vkCreateDescriptorSetLayout(h.device,&li,nullptr,&p.sets[1]));
  VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,pushSize};
  VkPipelineLayoutCreateInfo lc{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  lc.setLayoutCount=2; lc.pSetLayouts=p.sets; lc.pushConstantRangeCount=1; lc.pPushConstantRanges=&range;
  VK(vkCreatePipelineLayout(h.device,&lc,nullptr,&p.layout));
  p.module=h.shader(code);
  VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,p.module,"main",nullptr};
  ci.layout=p.layout;
  VK(vkCreateComputePipelines(h.device,VK_NULL_HANDLE,1,&ci,nullptr,&p.pipeline));
  VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,image?1u:2u},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,image?1u:0u}};
  VkDescriptorPoolCreateInfo pc{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pc.maxSets=2; pc.poolSizeCount=image?2:1; pc.pPoolSizes=sizes;
  VK(vkCreateDescriptorPool(h.device,&pc,nullptr,&p.pool));
  VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  ai.descriptorPool=p.pool; ai.descriptorSetCount=2; ai.pSetLayouts=p.sets;
  VK(vkAllocateDescriptorSets(h.device,&ai,p.ds));
  VkDescriptorImageInfo ii{}; ii.imageView=source.view; ii.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorBufferInfo bi[2]={{input.b,0,input.size},{output.b,0,output.size}};
  VkWriteDescriptorSet w[2]{};
  for (unsigned i=0;i<2;i++) {w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[i].dstSet=p.ds[i];
    w[i].dstBinding=0; w[i].descriptorCount=1; w[i].descriptorType=(i==0&&image)?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;}
  if(image) w[0].pImageInfo=&ii; else w[0].pBufferInfo=&bi[0];
  w[1].pBufferInfo=&bi[1]; vkUpdateDescriptorSets(h.device,2,w,0,nullptr);
  return p;
}
static void dispatch(Harness& h, Pipeline& p, const void* constants, uint32_t size, uint32_t gx,uint32_t gy) {
  vkCmdBindPipeline(h.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
  vkCmdBindDescriptorSets(h.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,2,p.ds,0,nullptr);
  vkCmdPushConstants(h.cmd,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,size,constants);
  vkCmdDispatch(h.cmd,gx,gy,1);
}
int main(int argc,char** argv) {
 try {
  if(argc!=2) throw std::runtime_error("usage: direct-color-gpu kernel.spv");
  auto code=Lb2BuildDirectColorResolveShader();
  std::ofstream spv(argv[1],std::ios::binary); spv.write((const char*)code.data(),code.size()*4); spv.close();
  Harness h; h.init(VK_FORMAT_R8G8B8A8_UNORM);
  constexpr uint32_t W=880,H=496, TILEPITCH=11, CAP=3*1024*1024;
  auto staging=h.buffer(W*H*4,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  auto edram=h.buffer(TILEPITCH*((H+15)/16)*1280*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  auto direct=h.buffer(CAP,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  auto baseline=h.buffer(CAP,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  memset(edram.p,0,edram.size);
  for(uint32_t y=0;y<H;y++) for(uint32_t x=0;x<W;x++) {
    uint32_t v=pixel(x,y); memcpy(staging.p+(y*W+x)*4,&v,4);
    uint32_t tile=(y/16)*TILEPITCH+x/80;
    uint32_t index=tile*1280+(y%16)*80+x%80; memcpy(edram.p+index*4,&v,4);
  }
  auto image=h.image(W,H,VK_SAMPLE_COUNT_1_BIT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
  image.view=h.view(image,VK_IMAGE_ASPECT_COLOR_BIT);
  h.begin();
  h.barrier(image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={W,H,1};
  vkCmdCopyBufferToImage(h.cmd,staging.b,image.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
  h.barrier(image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  h.submit();
  std::vector<uint32_t> sdk(std::begin(resolve_fast_32bpp_1x2xmsaa_cs),std::end(resolve_fast_32bpp_1x2xmsaa_cs));
  auto dp=makePipeline(h,code,true,40,image,edram,direct);
  auto bp=makePipeline(h,sdk,false,20,image,edram,baseline);
  struct Case {uint32_t sx,sy,w,h,dx,dy,pitch,base;};
  const Case cases[]={{0,0,8,8,0,0,32,256},{8,8,32,24,8,8,64,512},
    {16,8,64,16,24,16,96,256},{8,0,72,32,8,24,128,1024},
    {0,0,848,480,0,0,864,256},{88,24,128,40,8,8,160,256},
    {160,32,32,16,8,8,64,256},{3,5,1,1,7,3,32,256},
    {5,7,17,11,13,9,64,512},{2,3,31,13,9,5,64,256},
    {3,5,121,17,7,3,137,256},{0,0,848,480,0,0,849,512}};
  uint64_t checked=0, count=0, sdk_count=0;
  for(auto c:cases) for(uint32_t endian=0;endian<4;endian++) for(uint32_t swap=0;swap<2;swap++) {
    memset(direct.p,0xcd,CAP); memset(baseline.p,0xcd,CAP);
    std::vector<uint8_t> expected(CAP,0xcd);
    for(uint32_t y=0;y<c.h;y++) for(uint32_t x=0;x<c.w;x++) {
      uint32_t at=c.base+tiled(c.dx+x,c.dy+y,c.pitch);
      if(at+4>CAP) throw std::runtime_error("oracle out of range");
      uint32_t v=convert(pixel(c.sx+x,c.sy+y),endian,swap); memcpy(expected.data()+at,&v,4);
    }
    Lb2DirectColorPush pc{c.sx,c.sy,c.w,c.h,c.dx,c.dy,c.pitch,c.base,endian,swap};
    bool sdk_eligible = !(c.sx%8 || c.sy%8 || c.w%8 || c.h%8 || c.dx%8 || c.dy%8 || c.pitch%32);
    rex::graphics::draw_util::ResolveCopyShaderConstants sc{};
    sc.dest_relative.edram_info.pitch_tiles=TILEPITCH;
    sc.dest_relative.edram_info.base_tiles=(c.sy/16)*TILEPITCH+c.sx/80;
    sc.dest_relative.coordinate_info.edram_offset_x_div_8=(c.sx%80)/8;
    sc.dest_relative.coordinate_info.edram_offset_y_div_8=(c.sy%16)/8;
    sc.dest_relative.coordinate_info.width_div_8=c.w/8;
    sc.dest_relative.coordinate_info.draw_resolution_scale_x=1;
    sc.dest_relative.coordinate_info.draw_resolution_scale_y=1;
    sc.dest_relative.dest_info.value=0;
    sc.dest_relative.dest_info.copy_dest_endian=static_cast<rex::graphics::xenos::Endian128>(endian);
    sc.dest_relative.dest_info.copy_dest_swap=swap;
    sc.dest_relative.dest_coordinate_info.pitch_aligned_div_32=(c.pitch+31)/32;
    sc.dest_relative.dest_coordinate_info.offset_x_div_8=c.dx/8;
    sc.dest_relative.dest_coordinate_info.offset_y_div_8=c.dy/8;
    sc.dest_base=c.base;
    h.begin();
    h.bufferBarrier(edram,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    h.bufferBarrier(direct,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    h.bufferBarrier(baseline,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    dispatch(h,dp,&pc,sizeof(pc),(c.w+7)/8,(c.h+7)/8);
    if(sdk_eligible) dispatch(h,bp,&sc,sizeof(sc),(c.w+63)/64,(c.h+7)/8);
    h.bufferBarrier(direct,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT);
    h.bufferBarrier(baseline,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT);
    h.submit();
    for(uint32_t i=0;i<CAP;i++) if(direct.p[i]!=expected[i] || (sdk_eligible && baseline.p[i]!=expected[i])) {
      printf("FAIL case=%llu src=%u,%u size=%ux%u dst=%u,%u pitch=%u endian=%u swap=%u offset=%u expected=%02x direct=%02x sdk=%02x\n",
      (unsigned long long)count,c.sx,c.sy,c.w,c.h,c.dx,c.dy,c.pitch,endian,swap,i,expected[i],direct.p[i],baseline.p[i]); return 1;
    }
    checked+=CAP; count++; sdk_count+=sdk_eligible;
  }
  printf("PASS production_cases=%llu production_bytes=%llu sdk_cases=%llu sdk_bytes=%llu (full buffers including canaries)\n",(unsigned long long)count,(unsigned long long)checked,(unsigned long long)sdk_count,(unsigned long long)(sdk_count*CAP));
  return 0;
 } catch(const std::exception& e) {fprintf(stderr,"ERROR: %s\n",e.what());return 1;}
}

