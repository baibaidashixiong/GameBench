#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gamebench::runtime {

class ArenaAllocator final {
public:
  explicit ArenaAllocator(std::size_t capacity_bytes);

  [[nodiscard]] auto allocate(std::size_t size_bytes, std::size_t alignment)
      -> std::span<std::byte>;
  void reset();
  [[nodiscard]] auto used_bytes() const -> std::size_t;
  [[nodiscard]] auto capacity_bytes() const -> std::size_t;

private:
  std::vector<std::byte> storage_;
  std::size_t offset_{};
};

class PoolAllocator final {
public:
  PoolAllocator(std::size_t block_size, std::size_t block_count);

  [[nodiscard]] auto allocate() -> void*;
  void deallocate(void* pointer) noexcept;
  [[nodiscard]] auto in_use() const -> std::size_t;
  [[nodiscard]] auto capacity() const -> std::size_t;

private:
  std::vector<std::byte> storage_;
  std::vector<std::size_t> free_blocks_;
  std::vector<bool> occupied_;
  std::size_t stride_{};
  std::size_t in_use_{};
};

} // namespace gamebench::runtime
