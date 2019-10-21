#include <gdul\WIP_job_handler\worker_impl.h>
#include <cassert>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gdul
{
namespace job_handle_detail
{

worker_impl::worker_impl()
	: myAutoCoreAffinity(0)
	, myCoreAffinity(0)
	, myIsRunning(false)
	, myPriorityDistributionIteration(0)
	, myQueueAffinity(0)
	, mySleepThreshhold(0)
	, myThreadHandle(nullptr)
{
	myThreadHandle = get_thread_handle();
}
worker_impl::worker_impl(std::thread&& thread, std::uint8_t coreAffinity)
	: myAutoCoreAffinity(coreAffinity)
	, myThread(std::move(thread))
	, myCoreAffinity(0)
	, myIsRunning(true)
	, myPriorityDistributionIteration(0)
	, myQueueAffinity(job_handler_detail::Worker_Auto_Affinity)
	, mySleepThreshhold(250)
	, myThreadHandle(nullptr)
{
	set_core_affinity(coreAffinity);
}

worker_impl::~worker_impl()
{
}

worker_impl & worker_impl::operator=(worker_impl && other)
{
	myAutoCoreAffinity = other.myAutoCoreAffinity;
	myThread = std::move(other.myThread);
	myCoreAffinity = other.myCoreAffinity;
	myIsRunning.store(other.myIsRunning.load(std::memory_order_relaxed), std::memory_order_relaxed);
	myThreadHandle = other.myThreadHandle;
	mySleepThreshhold = other.mySleepThreshhold;
	mySleepTimer = other.mySleepTimer;
	myPriorityDistributionIteration = other.myPriorityDistributionIteration;
	myQueueAffinity = other.myQueueAffinity;
	myLastJobTimepoint = other.myLastJobTimepoint;

	return *this;
}

void worker_impl::set_core_affinity(std::uint8_t core)
{
	assert(!is_retired() && "Cannot set affinity to inactive worker");

	if (core == job_handler_detail::Worker_Auto_Affinity) {
		myCoreAffinity = myAutoCoreAffinity;
	}

	const uint8_t core_(core % std::thread::hardware_concurrency());

	job_handler_detail::set_thread_core_affinity(core, myThreadHandle);
}
void worker_impl::set_queue_affinity(std::uint8_t queue)
{
	myQueueAffinity = queue;
}
void worker_impl::set_execution_priority(std::uint32_t priority)
{
	assert(!is_retired() && "Cannot set execution priority to inactive worker");

	job_handler_detail::set_thread_priority(priority, myThreadHandle);
}
void worker_impl::set_sleep_threshhold(std::uint16_t ms)
{
	mySleepThreshhold = ms;
}
void worker_impl::set_thread_handle(HANDLE handle)
{
	myThreadHandle = handle;
}
bool worker_impl::retire()
{
	return myIsRunning.exchange(false, std::memory_order_relaxed);
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
bool worker_impl::is_retired() const
{
	return myIsRunning.load(std::memory_order_relaxed);
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
std::uint8_t worker_impl::get_queue_affinity()
{
	if (myQueueAffinity == job_handler_detail::Worker_Auto_Affinity) {
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
void worker_impl::set_name(const char * name)
{
	assert(!is_retired() && "Cannot set name to inactive worker");

	job_handler_detail::set_thread_name(name, myThreadHandle);
}
}
}