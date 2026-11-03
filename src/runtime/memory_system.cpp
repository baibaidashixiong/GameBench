#include "runtime/memory_system.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace gamebench::runtime {
namespace {

[[nodiscard]] auto align_up(std::size_t value, std::size_t alignment) -> std::size_t {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    throw std::invalid_argument("内存对齐必须是 2 的幂");
  }
  const auto remainder = value % alignment;
  return remainder == 0 ? value : value + alignment - remainder;
}

} // namespace

ArenaAllocator::ArenaAllocator(std::size_t capacity_bytes) : storage_(capacity_bytes) {
  if (capacity_bytes == 0) {
    throw std::invalid_argument("Arena 容量必须大于零");
  }
}

auto ArenaAllocator::allocate(std::size_t size_bytes, std::size_t alignment)
    -> std::span<std::byte> {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    throw std::invalid_argument("内存对齐必须是 2 的幂");
  }
  const auto base = reinterpret_cast<std::uintptr_t>(storage_.data());
  const auto current = base + offset_;
  const auto alignment_mask = ~(static_cast<std::uintptr_t>(alignment) - 1U);
  const auto aligned_address = (current + alignment - 1U) & alignment_mask;
  const auto aligned_offset = static_cast<std::size_t>(aligned_address - base);
  if (size_bytes > storage_.size() || aligned_offset > storage_.size() - size_bytes) {
    throw std::bad_alloc();
  }
  auto result = std::span<std::byte>{storage_}.subspan(aligned_offset, size_bytes);
  offset_ = aligned_offset + size_bytes;
  return result;
}

void ArenaAllocator::reset() {
  offset_ = 0;
}

auto ArenaAllocator::used_bytes() const -> std::size_t {
  return offset_;
}

auto ArenaAllocator::capacity_bytes() const -> std::size_t {
  return storage_.size();
}

PoolAllocator::PoolAllocator(std::size_t block_size, std::size_t block_count) {
  if (block_size == 0 || block_count == 0) {
    throw std::invalid_argument("Pool 块大小和数量必须大于零");
  }
  stride_ = align_up(block_size, alignof(std::max_align_t));
  storage_.resize(stride_ * block_count);
  occupied_.resize(block_count);
  free_blocks_.reserve(block_count);
  for (std::size_t index = 0; index < block_count; ++index) {
    free_blocks_.push_back(block_count - index - 1);
  }
}

auto PoolAllocator::allocate() -> void* {
  if (free_blocks_.empty()) {
    return nullptr;
  }
  const auto block = free_blocks_.back();
  free_blocks_.pop_back();
  occupied_[block] = true;
  ++in_use_;
  return storage_.data() + block * stride_;
}

void PoolAllocator::deallocate(void* pointer) noexcept {
  if (pointer == nullptr) {
    return;
  }
  const auto* byte_pointer = static_cast<std::byte*>(pointer);
  if (byte_pointer < storage_.data() || byte_pointer >= storage_.data() + storage_.size()) {
    return;
  }
  const auto offset = static_cast<std::size_t>(byte_pointer - storage_.data());
  if (offset % stride_ != 0) {
    return;
  }
  const auto block = offset / stride_;
  if (block >= occupied_.size() || !occupied_[block]) {
    return;
  }
  occupied_[block] = false;
  free_blocks_.push_back(block);
  --in_use_;
}

auto PoolAllocator::in_use() const -> std::size_t {
  return in_use_;
}

auto PoolAllocator::capacity() const -> std::size_t {
  return occupied_.size();
}

} // namespace gamebench::runtime
