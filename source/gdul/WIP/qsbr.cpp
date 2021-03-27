#include "qsbr.h"

#include <cstdint>
#include <thread>
#include <atomic>
#include <cassert>

#pragma warning(push)
// Alignment padding
#pragma warning(disable:4324)

namespace gdul::qsbr {

static_assert(!((sizeof(std::size_t) * 8) < MaxThreads), "Max threads cannot exceed bits in std::size_t");

namespace qsbr_detail {

struct alignas(std::hardware_destructive_interference_size) qsb_tracker
{
	std::atomic<std::size_t> ix = 0;
	std::atomic<bool> inUse = 0;
};

struct g_container
{
	qsb_tracker trackers[MaxThreads]{};
	std::atomic<std::int8_t> lastTrackerIndex = 0;
};

struct t_container
{
	std::size_t viewedIterations[MaxThreads]{};
	std::size_t lastIx = 0;
	std::int8_t index = -1;
};

g_container g_states;
thread_local t_container t_states;

std::size_t update_mask(std::size_t existingMask)
{
	std::size_t mask(0);
	for (std::size_t maskProbe = existingMask, i = 0; maskProbe; maskProbe >>= 1, ++i) {
		if (maskProbe & 1) {
			const std::size_t previous(t_states.viewedIterations[i]);
			const std::size_t current(g_states.trackers[i].ix.load(std::memory_order_acquire));
			const std::size_t even(current & 1ull);
			const std::size_t changed(previous == current);
			const std::size_t evenBit(even << i);
			const std::size_t changeBit(changed << i);

			t_states.viewedIterations[i] = current;

			mask |= (changeBit & evenBit);
		}
	}

	return mask;
}

std::size_t create_new_mask()
{
	const std::uint8_t lastTrackerIndex(g_states.lastTrackerIndex.load(std::memory_order_acquire));
	const std::uint8_t lastTrackedBit(std::size_t(lastTrackerIndex) + 1);
	std::size_t initialTrackingMask(std::numeric_limits<std::size_t>::max());
	initialTrackingMask >>= (sizeof(shared_snapshot) * 8 /*shift away all unused bits*/) - lastTrackedBit;
	initialTrackingMask &= ~(std::size_t(1) << t_states.index);

	std::size_t mask(0);
	for (std::size_t maskProbe = initialTrackingMask, i = 0; maskProbe; maskProbe >>= 1, ++i) {
		if (maskProbe & 1) {
			const std::size_t current(g_states.trackers[i].ix);
			const std::size_t even(current & 1ull);
			const std::size_t evenBit(even << i);

			t_states.viewedIterations[i] = current;

			mask |= evenBit;
		}
	}

	return mask;
}

void register_thread()
{
	if (t_states.index != -1) {
		return;
	}

	// Find unused tracker slot
	for (std::int8_t i = 0; i < MaxThreads; ++i) {
		if (!g_states.trackers[i].inUse.load(std::memory_order_acquire)) {
			if (!g_states.trackers[i].inUse.exchange(true, std::memory_order_release)) {
				t_states.index = i;
				break;
			}
		}
	}

	assert(t_states.index != -1 && "Could not find slot for thread. Max threads exceeded?");

	// query_and_update last tracker index
	std::int8_t lastTrackerIndex(g_states.lastTrackerIndex.load(std::memory_order_relaxed));

	while (lastTrackerIndex < t_states.index) {
		if (g_states.lastTrackerIndex.compare_exchange_weak(lastTrackerIndex, t_states.index, std::memory_order_relaxed)) {
			break;
		}
	}
}

void unregister_thread()
{
	const std::int8_t index(t_states.index);

	assert((g_states.trackers[index].ix & 1ull) == 0 && "Cannot unregister thread from within critical section");

	if (index == -1) {
		return;
	}

	g_states.trackers[index].inUse.store(false, std::memory_order_relaxed);

	t_states.index = -1;
}

bool query_and_update(shared_snapshot& item)
{
	const std::size_t mask(item.m_state.load(std::memory_order_relaxed));

	if (!mask) {
		return true;
	}

	const std::size_t newMask(update_mask(mask));

	item.m_state.fetch_and(newMask, std::memory_order_relaxed);

	return !newMask;
}

bool initialize(shared_snapshot& item)
{
	const std::size_t mask(create_new_mask());
	item.m_state.store(mask, std::memory_order_release);
	return !mask;
}

bool query(const shared_snapshot& item)
{
	return !item.m_state.load(std::memory_order_relaxed);
}

void reset(shared_snapshot& item)
{
	item.m_state.store(std::numeric_limits<std::size_t>::max(), std::memory_order_relaxed);
}

}

void register_thread()
{
	qsbr_detail::register_thread();
}

void unregister_thread()
{
	qsbr_detail::unregister_thread();
}

bool initialize(shared_snapshot& item)
{
	return qsbr_detail::initialize(item);
}

bool query_and_update(shared_snapshot& item)
{
	return qsbr_detail::query_and_update(item);
}

bool query(const shared_snapshot& item)
{
	return qsbr_detail::query(item);
}

void reset(shared_snapshot& item)
{
	qsbr_detail::reset(item);
}

critical_section::critical_section()
	: m_nextIx(qsbr_detail::t_states.lastIx += 2)
	, m_globalIndex(qsbr_detail::t_states.index)
	, m_tracker(qsbr_detail::g_states.trackers[m_globalIndex].ix)
{
	assert((qsbr_detail::g_states.trackers[qsbr_detail::t_states.index].ix & 1ull) == 0 && "Cannot create critical section inside another critical section");
	assert(m_globalIndex != -1 && "Thread not registered");

	m_tracker.store(m_nextIx - 1, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_acq_rel);

}
critical_section::~critical_section()
{
	m_tracker.store(m_nextIx, std::memory_order_release);
}
}
#pragma warning(pop)