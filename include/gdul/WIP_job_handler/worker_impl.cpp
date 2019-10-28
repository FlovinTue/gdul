#include <gdul\WIP_job_handler\worker_impl.h>
#include <cassert>
#include <cmath>

#if defined(_WIN64) | defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace gdul
{
namespace job_handler_detail
{

worker_impl::worker_impl()
	: myAutoCoreAffinity(Worker_Auto_Affinity)
	, myCoreAffinity(Worker_Auto_Affinity)
	, myIsEnabled(false)
	, myPriorityDistributionIteration(0)
	, myQueueAffinity(Worker_Auto_Affinity)
	, mySleepThreshhold(std::numeric_limits<std::uint16_t>::max())
	, myThreadHandle(job_handler_detail::create_thread_handle())
	, myIsActive(false)
{
}
worker_impl::worker_impl(std::thread&& thread, std::uint8_t coreAffinity)
	: myAutoCoreAffinity(coreAffinity)
	, myThread(std::move(thread))
	, myCoreAffinity(Worker_Auto_Affinity)
	, myIsEnabled(false)
	, myPriorityDistributionIteration(0)
	, myQueueAffinity(Worker_Auto_Affinity)
	, mySleepThreshhold(250)
	, myThreadHandle(myThread.native_handle())
	, myIsActive(false)
{
	job_handler_detail::set_thread_core_affinity(coreAffinity, myThreadHandle);

	myIsActive.store(true, std::memory_order_release);
}

worker_impl::~worker_impl()
{
	if (myThread.get_id() == std::thread().get_id()) {
		CloseHandle(myThreadHandle);
		myThreadHandle = nullptr;
	}

	deactivate();

	if (myThread.joinable()) {
		myThread.join();
	}
}
worker_impl & worker_impl::operator=(worker_impl && other) noexcept
{
	myAutoCoreAffinity = other.myAutoCoreAffinity;
	myThread.swap(other.myThread);
	myCoreAffinity = other.myCoreAffinity;
	myIsEnabled.store(other.myIsEnabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
	std::swap(myThreadHandle, other.myThreadHandle);
	mySleepThreshhold = other.mySleepThreshhold;
	mySleepTimer = other.mySleepTimer;
	myPriorityDistributionIteration = other.myPriorityDistributionIteration;
	myQueueAffinity = other.myQueueAffinity;
	myLastJobTimepoint = other.myLastJobTimepoint;
	myIsActive.store(other.myIsActive.load(std::memory_order_relaxed), std::memory_order_release);

	return *this;
}

void worker_impl::set_core_affinity(std::uint8_t core)
{
	assert(is_active() && "Cannot set affinity to inactive worker");

	myCoreAffinity = core;

	if (myCoreAffinity == job_handler_detail::Worker_Auto_Affinity) {
		myCoreAffinity = myAutoCoreAffinity;
	}

	const uint8_t core_(myCoreAffinity % std::thread::hardware_concurrency());

	job_handler_detail::set_thread_core_affinity(core_, myThreadHandle);
}
void worker_impl::set_queue_affinity(std::uint8_t queue)
{
	myQueueAffinity = queue;
}
void worker_impl::set_execution_priority(std::uint32_t priority)
{
	assert(is_active() && "Cannot set execution priority to inactive worker");

	job_handler_detail::set_thread_priority(priority, myThreadHandle);
}
void worker_impl::set_sleep_threshhold(std::uint16_t ms)
{
	mySleepThreshhold = ms;
}
void worker_impl::activate()
{
	myIsEnabled.store(true, std::memory_order_release);
}
bool worker_impl::deactivate()
{
	return myIsActive.exchange(false, std::memory_order_release);
}
void worker_impl::refresh_sleep_timer()
{
	myLastJobTimepoint = mySleepTimer.now();
}
bool worker_impl::is_sleepy() const
{
	const std::chrono::high_resolution_clock::time_point current(mySleepTimer.now());
	const std::chrono::high_resolution_clock::time_point delta(current - myLastJobTimepoint);

	return !(std::chrono::duration_cast<std::chrono::milliseconds>(current - delta).count() < mySleepThreshhold);
}
bool worker_impl::is_active() const
{
	return myIsActive.load(std::memory_order_relaxed) & myIsEnabled.load(std::memory_order_relaxed);
}
bool worker_impl::is_enabled() const
{
	return myIsEnabled.load(std::memory_order_relaxed);
}
void worker_impl::idle()
{
	if (is_sleepy()) {
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
	else {
		std::this_thread::yield();
	}
}
// Maybe find some way to stick around at an index for a while? Better for cache...
// Also, maybe avoid retrying at a failed index twice in a row.
std::uint8_t worker_impl::get_queue_target()
{
	if (myQueueAffinity != job_handler_detail::Worker_Auto_Affinity) {
		return myQueueAffinity;
	}

	constexpr std::size_t totalDistributionChunks(job_handler_detail::pow2summation(1, job_handler_detail::Priority_Granularity));

	const std::size_t iteration(++myPriorityDistributionIteration);

	uint8_t index(0);


	// Find way to remove loop.
	// Maybe find the highest mod in one check somehow?
	for (uint8_t i = 1; i < job_handler_detail::Priority_Granularity; ++i) {
		const std::uint8_t power(((job_handler_detail::Priority_Granularity) - (i + 1)));
		const float fdesiredSlice(std::powf((float)2, (float)power));
		const std::size_t desiredSlice((std::size_t)fdesiredSlice);
		const std::size_t awardedSlice((totalDistributionChunks) / desiredSlice);
		const uint8_t prev(index);
		const uint8_t eval(iteration % awardedSlice == 0);
		index += eval * i;
		index -= prev * eval;
	}

	return index;
}
std::uint8_t worker_impl::get_fetch_retries() const
{
	return myQueueAffinity == job_handler_detail::Worker_Auto_Affinity ? job_handler_detail::Priority_Granularity : 1;
}
void worker_impl::set_name(const char * name)
{
	assert(is_active() && "Cannot set name to inactive worker");

	job_handler_detail::set_thread_name(name, myThreadHandle);
}
}
}