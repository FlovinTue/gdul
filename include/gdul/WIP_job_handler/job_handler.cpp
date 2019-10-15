

#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\WIP_job_handler\job_impl.h>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gdul
{


#undef GetObject
#undef max

thread_local std::chrono::high_resolution_clock job_handler::ourSleepTimer;
thread_local std::chrono::high_resolution_clock::time_point job_handler::ourLastJobTimepoint;
thread_local size_t job_handler::ourLastJobSequence(0);

job_handler::job_handler()
	: myIdleJob([this]() { idle(); })
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
	
	myIsRunning = true;

	myWorkers.reserve(myInitInfo.myNumWorkers);

	for (std::uint32_t i = 0; i < info.myNumWorkers; ++i) {
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
		const job_handler_detail::job_impl* job(fetch_job());
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
	job returnValue(myIdleJob);

	const size_t size(myJobSequences.size());
	size_t currentIndex(ourLastJobSequence);
	for (size_t i = 0; i < size; ++i, ++currentIndex %= size) {
		job_sequence_impl* const currentSequence(myJobSequences[currentIndex]);

		if (currentSequence->fetch_job(returnValue)) {
			job_handler::this_JobSequence = currentSequence;
			ourLastJobSequence = currentIndex;
			break;
		}
	}
	return returnValue;
}


}