#include "qsbr.h"

#include <cstdint>
#include <thread>
#include <atomic>
#include <cassert>

#pragma warning(push)
// Alignment padding
#pragma warning(disable:4324)

namespace gdul::qsb {

namespace qsb_detail {

struct alignas(std::hardware_destructive_interference_size) qsb_tracker
{
	std::size_t iteration = 0;
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
	std::int8_t index = -1;
};

g_container g_items;
thread_local t_container t_items;

std::size_t create_initial_mask(std::size_t checkMask)
{
	std::size_t changeMask(0);
	for (std::size_t maskProbe = checkMask, i = 0; maskProbe; maskProbe >>= 1, ++i) {
		if (maskProbe & 1) {
			const std::size_t current(g_items.trackers[i].iteration);
			const std::size_t even(current % 2);
			const std::size_t evenBit(even << i);

			t_items.viewedIterations[i] = current;

			changeMask |= evenBit;
		}
	}

	return changeMask;
}

std::size_t update_mask(std::size_t checkMask)
{
	std::size_t changeMask(0);
	for (std::size_t maskProbe = checkMask, i = 0; maskProbe; maskProbe >>= 1, ++i) {
		if (maskProbe & 1) {
			const std::size_t previous(t_items.viewedIterations[i]);
			const std::size_t current(g_items.trackers[i].iteration);
			const std::size_t even(current % 2);
			const std::size_t changed(previous == current);
			const std::size_t evenBit(even << i);
			const std::size_t changeBit(changed << i);

			t_items.viewedIterations[i] = current;

			changeMask |= (changeBit & evenBit);
		}
	}

	return changeMask;
}

void quiescent_state()
{
	const std::int8_t index(t_items.index);

	assert(index != -1 && "Thread not registered");

	++g_items.trackers[index].iteration;
}

void register_thread()
{
	if (t_items.index != -1) {
		return;
	}

	// Find unused tracker slot
	for (std::int8_t i = 0; i < MaxThreads; ++i) {
		if (!g_items.trackers[i].inUse.load(std::memory_order_acquire)) {
			if (!g_items.trackers[i].inUse.exchange(true, std::memory_order_release)) {
				t_items.index = i;
				break;
			}
		}
	}

	assert(t_items.index != -1 && "Max threads exceeded");

	// Update last tracker index
	std::int8_t lastTrackerIndex(g_items.lastTrackerIndex.load(std::memory_order_acquire));

	while (lastTrackerIndex < t_items.index) {
		if (g_items.lastTrackerIndex.compare_exchange_weak(lastTrackerIndex, t_items.index, std::memory_order_release, std::memory_order_acquire)) {
			break;
		}
	}
}

void unregister_thread()
{
	const std::int8_t index(t_items.index);

	assert(g_items.trackers[index].iteration % 2 == 0 && "Cannot unregister thread from within critical section");

	if (index == -1) {
		return;
	}

	g_items.trackers[index].inUse.store(false, std::memory_order_relaxed);

	t_items.index = -1;
}

std::size_t create_new_mask()
{
	const std::uint8_t lastTrackerIndex(g_items.lastTrackerIndex.load(std::memory_order_acquire));
	const std::uint8_t lastTrackedBit(std::size_t(lastTrackerIndex) + 1);
	std::size_t initialTrackingMask(std::numeric_limits<std::size_t>::max());
	initialTrackingMask >>= (sizeof(std::size_t) * 8 /*shift away all unused bits*/) - lastTrackedBit;
	//initialTrackingMask &= ~(std::size_t(1) << t_items.index);

	const std::size_t mask(create_initial_mask(initialTrackingMask));

	return mask;
}

bool update_item(qs_item& item)
{
	if (!item.m_mask) {
		return true;
	}

	std::atomic_thread_fence(std::memory_order_acquire);

	item.m_mask = update_mask(item.m_mask);

	return !item.m_mask;
}
}

void register_thread()
{
	qsb_detail::register_thread();
}

void unregister_thread()
{
	qsb_detail::unregister_thread();
}

bool update_item(qs_item& item)
{
	return qsb_detail::update_item(item);
}

critical_section::critical_section()
{
	assert(qsb_detail::g_items.trackers[qsb_detail::t_items.index].iteration % 2 == 0 && "Cannot create critical section inside another critical section");
	qsb_detail::quiescent_state();
}
critical_section::~critical_section()
{
	qsb_detail::quiescent_state();
}
qs_item::qs_item()
	: m_mask(qsb_detail::create_new_mask())
{
}
}
#pragma warning(pop)