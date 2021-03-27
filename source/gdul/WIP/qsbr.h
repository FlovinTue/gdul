#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

namespace gdul::qsbr {

constexpr std::uint8_t MaxThreads = sizeof(std::size_t) * 8;

/// <summary>
/// Used to track
/// </summary>
class snapshot
{
public:
	std::size_t m_state;
};

class shared_snapshot
{
public:
	std::atomic<std::size_t> m_state;
};

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
/// Resets a snapshot to unverified state
/// </summary>
/// <param name="snapshot">snapshot item</param>
void reset(shared_snapshot& snapshot);

/// <summary>
/// Initialize snapshot
/// </summary>
/// <param name="snapshot">snapshot item</param>
/// <returns>True if no threads are inside critical sections</returns>
bool initialize(shared_snapshot& snapshot);

/// <summary>
/// Query other threads's state in relation to a snapshot and update it's state
/// </summary>
/// <param name="snapshot">snapshot item</param>
/// <returns>True if no threads are inside critical sections or if they have left the critical section they were in at snapshot initialization</returns>
bool query_and_update(shared_snapshot& snapshot);

/// <summary>
/// Query other threads's state in relation to a snapshot
/// </summary>
/// <param name="snapshot">snapshot item</param>
/// <returns>True if no threads are inside critical sections or if they have left the critical section they were in at snapshot initialization</returns>
bool query(const shared_snapshot& snapshot);

}