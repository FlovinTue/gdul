

#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\WIP_job_handler\job_impl.h>
#include <gdul\WIP_job_handler\job_impl_allocator.h>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gdul
{


#undef GetObject
#undef max

thread_local std::chrono::high_resolution_clock job_handler::ourSleepTimer;
thread_local std::chrono::high_resolution_clock::time_point job_handler::ourLastJobTimepoint;
thread_local std::size_t job_handler::ourPriorityDistributionIteration(0);
thread_local size_t job_handler::ourLastJobSequence(0);

job_handler::job_handler()
	: myJobImplChunkPool(128, myMainAllocator)
	, myJobImplAllocator(&myJobImplChunkPool)
	, myIdleJob(make_shared<job_handler_detail::job_impl, job_handler_detail::job_impl_allocator<uint8_t>>(myJobImplAllocator, [this]() {idle(); }))
	, myIsRunning(false)
	, myTotalQueueDistributionChunks(job_handler_detail::summation(1, job_handler_detail::Priority_Granularity))
{
}


job_handler::~job_handler()
{
	reset();
}

void job_handler::Init(const job_handler_info & info)
{
	myInitInfo = info;
	
	myIsRunning = true;

	myWorkers.reserve(myInitInfo.myMaxWorkers);

	for (std::uint32_t i = 0; i < info.myMaxWorkers; ++i) {
		myWorkers.push_back(std::thread([this, i]() { launch_worker(i); }));
	}
}

void job_handler::reset()
{
	myIsRunning = false;

	for (size_t i = 0; i < myWorkers.size(); ++i) {
		myWorkers[i].join();
	}

	myWorkers.clear();
	myWorkers.shrink_to_fit();

	myInitInfo = job_handler_info();
}

void job_handler::run()
{
	launch_worker(0);
}

void job_handler::abort()
{
	myIsRunning.store(false, std::memory_order_relaxed);
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
	while (myIsRunning) {
		job_handler_detail::job_impl* const job(fetch_job());
		(*job)();
	}
}
void job_handler::idle()
{
	//job_handler::this_job = nullptr

	const std::chrono::high_resolution_clock::time_point current(ourSleepTimer.now());
	const std::chrono::high_resolution_clock::time_point delta(current - ourLastJobTimepoint);

	if (std::chrono::duration_cast<std::chrono::milliseconds>(current - delta).count() < myInitInfo.mySleepThreshhold) {
		std::this_thread::yield();
	}
	else {
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
}
job_handler_detail::job_impl* job_handler::fetch_job()
{
	uint8_t queueIndex(0);
	job_handler_detail::job_impl* returnValue(myIdleJob);

	for (uint8_t i = 0; i < job_handler_detail::Priority_Granularity; ++i) {
		queueIndex = generate_priority_index();

		if (myJobQueues[queueIndex].try_pop(returnValue)) {
			break;
		}
	}
	return returnValue;
}

uint8_t job_handler::generate_priority_index()
{
	const std::size_t iteration(++ourPriorityDistributionIteration);
	const std::size_t queues(4);

	uint8_t index(0);

	for (uint8_t i = 1; i < queues; ++i) {
		const std::size_t desiredSlice(std::pow(2, (queues)-(i + 1)));
		const std::size_t awardedSlice((myTotalQueueDistributionChunks) / desiredSlice);
		const uint8_t prev(index);
		const uint8_t eval(iteration % awardedSlice == 0);
		index += eval * i;
		index -= prev * eval;
	}

	return index % 4;
}


}