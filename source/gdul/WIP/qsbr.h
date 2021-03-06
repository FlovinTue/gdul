#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

namespace gdul::qsbr {

constexpr std::uint8_t MaxThreads = 64;

/// <summary>
/// Used to track
/// </summary>
using qsb_item = std::atomic<std::size_t>;

/// <summary>
/// Use this to track presence of a thread within a critical section. Critical sections
/// distinguish from one another temporally so that each construction of the same critical section
/// will be uniquely identified
/// </summary>
class critical_section
{
public:
	critical_section();
	~critical_section();

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
/// Resets an item to unverified state
/// </summary>
/// <param name="item">Item state</param>
void reset(qsb_item& item);

/// <summary>
/// Initialize item
/// </summary>
/// <param name="item">Item state</param>
/// <returns>True if no threads are inside critical sections</returns>
bool set(qsb_item& item);

/// <summary>
/// Update item state
/// </summary>
/// <param name="item">Item state</param>
/// <returns>True if no threads are inside critical sections or if they have left the critical section they were in at item initialization</returns>
bool update(qsb_item& item);

/// <summary>
/// Check item state
/// </summary>
/// <param name="item">Item state</param>
/// <returns>True if no threads are inside critical sections or if they have left the critical section they were in at item initialization</returns>
bool is_safe(const qsb_item& item);

}