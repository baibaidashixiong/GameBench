#include "renderer/vulkan_context.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace gamebench::renderer {
namespace {

[[nodiscard]] auto result_name(VkResult result) -> const char* {
  switch (result) {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  default:
    return "VK_ERROR";
  }
}

void check(VkResult result, const char* operation) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::string{operation} + " 失败: " + result_name(result));
  }
}

[[nodiscard]] auto find_graphics_family(VkPhysicalDevice device, std::uint32_t& family) -> bool {
  std::uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> properties(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
  for (std::uint32_t index = 0; index < count; ++index) {
    if ((properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U &&
        (properties[index].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0U) {
      family = index;
      return true;
    }
  }
  return false;
}

[[nodiscard]] auto find_memory_type(VkPhysicalDevice device, std::uint32_t bits,
                                    VkMemoryPropertyFlags properties) -> std::uint32_t {
  VkPhysicalDeviceMemoryProperties memory{};
  vkGetPhysicalDeviceMemoryProperties(device, &memory);
  for (std::uint32_t index = 0; index < memory.memoryTypeCount; ++index) {
    if ((bits & (1U << index)) != 0U &&
        (memory.memoryTypes[index].propertyFlags & properties) == properties) {
      return index;
    }
  }
  throw std::runtime_error("未找到匹配的 Vulkan 内存类型");
}

} // namespace

struct VulkanContext::Impl {
  ~Impl() {
    if (device != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device);
      for (const auto fence : fences) {
        if (fence != VK_NULL_HANDLE) {
          vkDestroyFence(device, fence, nullptr);
        }
      }
      if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image, nullptr);
      }
      if (image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, image_memory, nullptr);
      }
      if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
      }
      if (buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffer_memory, nullptr);
      }
      for (const auto pool : pools) {
        if (pool != VK_NULL_HANDLE) {
          vkDestroyCommandPool(device, pool, nullptr);
        }
      }
      vkDestroyDevice(device, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
      vkDestroyInstance(instance, nullptr);
    }
  }

  VkInstance instance{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device{VK_NULL_HANDLE};
  VkDevice device{VK_NULL_HANDLE};
  VkQueue queue{VK_NULL_HANDLE};
  std::uint32_t queue_family{};
  std::vector<VkCommandPool> pools;
  std::vector<VkCommandBuffer> command_buffers;
  std::array<VkFence, 3> fences{};
  VkBuffer buffer{VK_NULL_HANDLE};
  VkDeviceMemory buffer_memory{VK_NULL_HANDLE};
  VkImage image{VK_NULL_HANDLE};
  VkDeviceMemory image_memory{VK_NULL_HANDLE};
  std::string device_name;
  std::string driver_name;
  std::uint32_t api_version{};

  void initialize_layout();
};

void VulkanContext::Impl::initialize_layout() {
  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = pools.front();
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  check(vkAllocateCommandBuffers(device, &allocate_info, &command_buffer),
        "vkAllocateCommandBuffers");

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer");

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  check(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  check(vkQueueSubmit(queue, 1, &submit_info, fences.front()), "vkQueueSubmit");
  check(vkWaitForFences(device, 1, &fences.front(), VK_TRUE,
                        std::numeric_limits<std::uint64_t>::max()),
        "vkWaitForFences");
  check(vkResetFences(device, 1, &fences.front()), "vkResetFences");
}

VulkanContext::VulkanContext(std::size_t command_pool_count) : impl_(std::make_unique<Impl>()) {
  if (command_pool_count == 0) {
    throw std::invalid_argument("Vulkan Command Pool 数量必须大于零");
  }

  VkApplicationInfo application{};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.pApplicationName = "GameBench";
  application.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
  application.pEngineName = "GameBench";
  application.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
  application.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo instance_info{};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &application;
  check(vkCreateInstance(&instance_info, nullptr, &impl_->instance), "vkCreateInstance");

  std::uint32_t device_count = 0;
  check(vkEnumeratePhysicalDevices(impl_->instance, &device_count, nullptr),
        "vkEnumeratePhysicalDevices");
  if (device_count == 0) {
    throw std::runtime_error("没有可用的 Vulkan 物理设备");
  }
  std::vector<VkPhysicalDevice> devices(device_count);
  check(vkEnumeratePhysicalDevices(impl_->instance, &device_count, devices.data()),
        "vkEnumeratePhysicalDevices");

  int best_score = -1;
  for (const auto candidate : devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(candidate, &properties);
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
      continue;
    }
    std::uint32_t family = 0;
    if (!find_graphics_family(candidate, family)) {
      continue;
    }
    const auto score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
    if (score > best_score) {
      best_score = score;
      impl_->physical_device = candidate;
      impl_->queue_family = family;
      impl_->device_name = properties.deviceName;
      impl_->api_version = properties.apiVersion;
    }
  }
  if (impl_->physical_device == VK_NULL_HANDLE) {
    throw std::runtime_error("没有可用的非 CPU Vulkan 物理设备");
  }

  VkPhysicalDeviceDriverProperties driver_properties{};
  driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
  VkPhysicalDeviceProperties2 properties2{};
  properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties2.pNext = &driver_properties;
  vkGetPhysicalDeviceProperties2(impl_->physical_device, &properties2);
  impl_->driver_name = driver_properties.driverName;

  constexpr float priority = 1.0F;
  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = impl_->queue_family;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;
  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  check(vkCreateDevice(impl_->physical_device, &device_info, nullptr, &impl_->device),
        "vkCreateDevice");
  vkGetDeviceQueue(impl_->device, impl_->queue_family, 0, &impl_->queue);

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = 16U * 1024U * 1024U;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  check(vkCreateBuffer(impl_->device, &buffer_info, nullptr, &impl_->buffer), "vkCreateBuffer");
  VkMemoryRequirements buffer_requirements{};
  vkGetBufferMemoryRequirements(impl_->device, impl_->buffer, &buffer_requirements);
  VkMemoryAllocateInfo buffer_allocate{};
  buffer_allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  buffer_allocate.allocationSize = buffer_requirements.size;
  buffer_allocate.memoryTypeIndex =
      find_memory_type(impl_->physical_device, buffer_requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  check(vkAllocateMemory(impl_->device, &buffer_allocate, nullptr, &impl_->buffer_memory),
        "vkAllocateMemory(buffer)");
  check(vkBindBufferMemory(impl_->device, impl_->buffer, impl_->buffer_memory, 0),
        "vkBindBufferMemory");

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent = {256, 256, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  check(vkCreateImage(impl_->device, &image_info, nullptr, &impl_->image), "vkCreateImage");
  VkMemoryRequirements image_requirements{};
  vkGetImageMemoryRequirements(impl_->device, impl_->image, &image_requirements);
  VkMemoryAllocateInfo image_allocate{};
  image_allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  image_allocate.allocationSize = image_requirements.size;
  image_allocate.memoryTypeIndex =
      find_memory_type(impl_->physical_device, image_requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  check(vkAllocateMemory(impl_->device, &image_allocate, nullptr, &impl_->image_memory),
        "vkAllocateMemory(image)");
  check(vkBindImageMemory(impl_->device, impl_->image, impl_->image_memory, 0),
        "vkBindImageMemory");

  impl_->pools.resize(command_pool_count, VK_NULL_HANDLE);
  impl_->command_buffers.resize(command_pool_count, VK_NULL_HANDLE);
  for (std::size_t index = 0; index < command_pool_count; ++index) {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = impl_->queue_family;
    check(vkCreateCommandPool(impl_->device, &pool_info, nullptr, &impl_->pools[index]),
          "vkCreateCommandPool");
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = impl_->pools[index];
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;
    check(vkAllocateCommandBuffers(impl_->device, &allocate_info, &impl_->command_buffers[index]),
          "vkAllocateCommandBuffers");
  }

  for (auto& fence : impl_->fences) {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    check(vkCreateFence(impl_->device, &fence_info, nullptr, &fence), "vkCreateFence");
  }
  impl_->initialize_layout();
}

VulkanContext::~VulkanContext() = default;
VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
auto VulkanContext::operator=(VulkanContext&&) noexcept -> VulkanContext& = default;

void VulkanContext::record_no_submit(std::size_t pool_index, std::uint32_t command_count,
                                     std::uint32_t seed) {
  if (pool_index >= impl_->pools.size()) {
    throw std::out_of_range("Vulkan Command Pool 索引越界");
  }
  auto& pool = impl_->pools[pool_index];
  auto& command_buffer = impl_->command_buffers[pool_index];
  check(vkResetCommandPool(impl_->device, pool, 0), "vkResetCommandPool");
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer");
  for (std::uint32_t command = 0; command < command_count; ++command) {
    const auto offset = static_cast<VkDeviceSize>((seed + command) % 1024U) * 4096U;
    vkCmdFillBuffer(command_buffer, impl_->buffer, offset, sizeof(std::uint32_t), seed + command);
  }
  check(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");
}

void VulkanContext::record_offscreen(std::size_t pool_index, std::uint32_t command_count,
                                     std::uint32_t seed) {
  if (pool_index >= impl_->pools.size()) {
    throw std::out_of_range("Vulkan Command Pool 索引越界");
  }
  auto& pool = impl_->pools[pool_index];
  auto& command_buffer = impl_->command_buffers[pool_index];
  check(vkResetCommandPool(impl_->device, pool, 0), "vkResetCommandPool");
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer");

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = impl_->image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  VkClearColorValue color{};
  color.float32[0] = static_cast<float>(seed & 0xffU) / 255.0F;
  color.float32[1] = static_cast<float>((seed >> 8U) & 0xffU) / 255.0F;
  color.float32[2] = static_cast<float>((seed >> 16U) & 0xffU) / 255.0F;
  color.float32[3] = 1.0F;
  VkImageSubresourceRange range{};
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.levelCount = 1;
  range.layerCount = 1;
  for (std::uint32_t command = 0; command < command_count; ++command) {
    color.float32[0] = static_cast<float>((seed + command) & 0xffU) / 255.0F;
    vkCmdClearColorImage(command_buffer, impl_->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color,
                         1, &range);
  }
  check(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");
}

void VulkanContext::submit() {
  if (impl_->command_buffers.empty()) {
    return;
  }
  const auto batches = std::min<std::size_t>(impl_->fences.size(), impl_->command_buffers.size());
  std::vector<std::vector<VkCommandBuffer>> submitted(batches);
  for (std::size_t index = 0; index < impl_->command_buffers.size(); ++index) {
    submitted[index % batches].push_back(impl_->command_buffers[index]);
  }
  for (std::size_t batch = 0; batch < batches; ++batch) {
    check(vkResetFences(impl_->device, 1, &impl_->fences[batch]), "vkResetFences");
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = static_cast<std::uint32_t>(submitted[batch].size());
    submit_info.pCommandBuffers = submitted[batch].data();
    check(vkQueueSubmit(impl_->queue, 1, &submit_info, impl_->fences[batch]), "vkQueueSubmit");
  }
}

void VulkanContext::wait() {
  const auto batches = std::min<std::size_t>(impl_->fences.size(), impl_->command_buffers.size());
  for (std::size_t batch = 0; batch < batches; ++batch) {
    check(vkWaitForFences(impl_->device, 1, &impl_->fences[batch], VK_TRUE,
                          std::numeric_limits<std::uint64_t>::max()),
          "vkWaitForFences");
  }
}

auto VulkanContext::device_name() const -> const std::string& {
  return impl_->device_name;
}

auto VulkanContext::driver_name() const -> const std::string& {
  return impl_->driver_name;
}

auto VulkanContext::api_version() const -> std::uint32_t {
  return impl_->api_version;
}

} // namespace gamebench::renderer
