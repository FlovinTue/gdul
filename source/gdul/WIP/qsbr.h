#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

namespace gdul::qsbr {

constexpr std::uint8_t MaxThreads = sizeof(std::size_t) * 8 - 1 /* Reserve last bit for invalid state */;

/// <summary>
/// Once set, may be updated & queried using update and is_safe
/// </summary>
using item = std::atomic<std::size_t>;

/// <summary>
/// Use this to track critical section presence of this thread. Critical sections
/// distinguish from one another temporally so that each construction of the same critical section
/// will be uniquely identified
/// </summary>
class critical_section
{
public:
	critical_section();
	~critical_section();

private:
	const std::size_t m_nextIx;
	const std::int8_t m_globalIndex;
	std::atomic<std::size_t>& m_tracker;
};

/// <summary>
/// Register current thread to participate in critical section tracking
/// </summary>
void register_thread();

/// <summary>
/// Unregister current thread from critical section tracking
/// </summary>
void unregister_thread();

/// <summary>
/// Set an item to invalid state. Must be set to transition out of invalid state
/// </summary>
/// <param name="item">Item state</param>
void invalidate(item& item);

/// <summary>
/// Initialize item
/// </summary>
/// <param name="item">Item state</param>
/// <returns>True if no threads are inside critical sections</returns>
bool set(item& item);

/// <summary>
/// Update item state
/// </summary>
/// <param name="item">Item state</param>
/// <returns>True if no threads are inside critical sections or if they have left the critical section they were in at item initialization</returns>
bool update(item& item);

/// <summary>
/// Check item state
/// </summary>
/// <param name="item">Item state</param>
/// <returns>True if no threads are inside critical sections or if they have left the critical section they were in at item initialization</returns>
bool is_safe(const item& item);

}