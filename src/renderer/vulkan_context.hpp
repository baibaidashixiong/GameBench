#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace gamebench::renderer {

class VulkanContext final {
public:
  explicit VulkanContext(std::size_t command_pool_count);
  ~VulkanContext();

  VulkanContext(const VulkanContext&) = delete;
  auto operator=(const VulkanContext&) -> VulkanContext& = delete;
  VulkanContext(VulkanContext&&) noexcept;
  auto operator=(VulkanContext&&) noexcept -> VulkanContext&;

  void record_no_submit(std::size_t pool_index, std::uint32_t command_count, std::uint32_t seed);
  void record_offscreen(std::size_t pool_index, std::uint32_t command_count, std::uint32_t seed);
  void submit();
  void wait();

  [[nodiscard]] auto device_name() const -> const std::string&;
  [[nodiscard]] auto driver_name() const -> const std::string&;
  [[nodiscard]] auto api_version() const -> std::uint32_t;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace gamebench::renderer
