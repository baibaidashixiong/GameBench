#include "runtime/memory_system.hpp"

#include <gtest/gtest.h>

#include <cstddef>

TEST(MemorySystemTest, ArenaAllocatorAlignsAndResets) {
  gamebench::runtime::ArenaAllocator arena(4096);
  const auto first = arena.allocate(3, 16);
  const auto second = arena.allocate(8, 32);

  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(first.data()) % 16U, 0U);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second.data()) % 32U, 0U);
  EXPECT_GE(arena.used_bytes(), 11U);
  arena.reset();
  EXPECT_EQ(arena.used_bytes(), 0U);
}

TEST(MemorySystemTest, PoolAllocatorReusesReleasedBlocks) {
  gamebench::runtime::PoolAllocator pool(64, 2);
  void* first = pool.allocate();
  void* second = pool.allocate();
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(pool.allocate(), nullptr);
  EXPECT_EQ(pool.in_use(), 2U);

  pool.deallocate(first);
  EXPECT_EQ(pool.in_use(), 1U);
  EXPECT_EQ(pool.allocate(), first);
  EXPECT_EQ(pool.capacity(), 2U);
}
