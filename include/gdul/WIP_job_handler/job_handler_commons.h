#pragma once

#include <thread>

namespace gdul
{
namespace job_handler_detail
{
constexpr std::uint8_t Priority_Granularity = 4;
constexpr std::size_t Callable_Max_Size_No_Heap_Alloc = 24;

using allocator_type = std::allocator<std::uint8_t>;

void set_thread_name(const char* name);

constexpr std::size_t log2align(std::size_t value)
{
	std::size_t highBit(0);
	std::size_t shift(value);
	for (; shift; ++highBit) {
		shift >>= 1;
	}

	highBit -= static_cast<bool>(highBit);

	const std::size_t mask((std::size_t(1) << (highBit)) - 1);
	const std::size_t remainder((static_cast<bool>(value & mask)));

	const std::size_t sum(highBit + remainder);

	return std::size_t(1) << sum;
}
}
}