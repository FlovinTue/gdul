

#include <gdul\WIP_job_handler\job_handler.h>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gdul
{

#undef GetObject
#undef max

thread_local job job_handler::this_job(nullptr);
thread_local std::chrono::high_resolution_clock job_handler::ourSleepTimer;
thread_local std::chrono::high_resolution_clock::time_point job_handler::ourLastJobTimepoint;
thread_local std::size_t job_handler::ourPriorityDistributionIteration(0);
thread_local size_t job_handler::ourLastJobSequence(0);

job_handler::job_handler()
	: myJobImplChunkPool(job_handler_detail::Job_Impl_Allocator_Block_Size, myMainAllocator)
	, myJobImplAllocator(&myJobImplChunkPool)
	, myIsRunning(false)
{
}

job_handler::job_handler(allocator_type & allocator)
	: myMainAllocator(allocator)
	, myJobImplChunkPool(128, myMainAllocator)
	, myJobImplAllocator(&myJobImplChunkPool)
	, myIsRunning(false)
{
}


job_handler::~job_handler()
{
	reset();
}

void job_handler::Init(const job_handler_info & info)
{
	myInitInfo = info;
	
	myIsRunning.store(true, std::memory_order_relaxed);

	for (std::uint32_t i = 0; i < info.myMaxWorkers; ++i) {
		myWorkers[i] = (std::thread([this, i]() { launch_worker(i); }));
	}
}

void job_handler::reset()
{
	bool exp(true);
	if (!myIsRunning.compare_exchange_strong(exp, false, std::memory_order_relaxed)) {
		return;
	}

	for (size_t i = 0; i < myInitInfo.myMaxWorkers; ++i) {
		if (myWorkers[i].joinable()) {
			myWorkers[i].join();
		}
	}

	myInitInfo = job_handler_info();
}

void job_handler::enqueue_job(job_impl_shared_ptr job)
{
	const std::uint8_t priority(job->get_priority());

	myJobQueues[priority].push(std::move(job));
}

void job_handler::launch_worker(std::uint32_t workerIndex)
{
	job_handler_detail::set_thread_name(std::string("job_handler_thread# " + std::to_string(workerIndex)).c_str());
	SetThreadPriority(GetCurrentThread(), myInitInfo.myWorkerPriorities);

	ourLastJobTimepoint = ourSleepTimer.now();

	//myInitInfo.myOnThreadLaunch();

	const uint64_t affinityMask(1ULL << workerIndex);
	while (!SetThreadAffinityMask(GetCurrentThread(), affinityMask));

	work();

	//myInitInfo.myOnThreadExit();
}
void job_handler::work()
{
	while (myIsRunning.load(std::memory_order_relaxed)) {

		this_job = job(fetch_job());

		if (this_job.myImpl) {
			(*this_job.myImpl)();

			ourLastJobTimepoint = ourSleepTimer.now();
		}
		else {
			idle();
		}
	}
}
void job_handler::idle()
{
	const std::chrono::high_resolution_clock::time_point current(ourSleepTimer.now());
	const std::chrono::high_resolution_clock::time_point delta(current - ourLastJobTimepoint);

	if (std::chrono::duration_cast<std::chrono::milliseconds>(current - delta).count() < myInitInfo.mySleepThreshhold) {
		std::this_thread::yield();
	}
	else {
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
}
job_handler::job_impl_shared_ptr job_handler::fetch_job()
{
	const uint8_t queueIndex(generate_priority_index());

	job_handler::job_impl_shared_ptr out(nullptr);

	for (uint8_t i = 0; i < job_handler_detail::Priority_Granularity; ++i) {

		const uint8_t index((queueIndex + i) % job_handler_detail::Priority_Granularity);

		if (myJobQueues[index].try_pop(out)) {
			return out;
		}
	}

	return out;
}

// Maybe find some way to stick around at an index for a while? Better for cache...
// Also, maybe avoid retrying at a failed index twice in a row.
uint8_t job_handler::generate_priority_index()
{
	constexpr std::size_t totalDistributionChunks(job_handler_detail::pow2summation(1, job_handler_detail::Priority_Granularity));

	const std::size_t iteration(++ourPriorityDistributionIteration);

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


}