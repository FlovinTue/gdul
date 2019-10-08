#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\WIP_job_handler\job_sequence.h>
#include <gdul\WIP_job_handler\job_sequence_impl.h>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gdul
{
namespace thread_naming {
	const DWORD MS_VC_EXCEPTION = 0x406D1388;
	#pragma pack(push,8)  
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; 
		LPCSTR szName; 
		DWORD dwThreadID; 
		DWORD dwFlags;  
	} THREADNAME_INFO;
	#pragma pack(pop)  
};

#undef GetObject
#undef max

thread_local std::chrono::high_resolution_clock job_handler::ourSleepTimer;
thread_local std::chrono::high_resolution_clock::time_point job_handler::ourLastJobTimepoint;
thread_local size_t job_handler::ourLastJobSequence(0);
thread_local job_sequence_impl* job_handler::this_JobSequence(nullptr);

job_handler::job_handler()
	: myIdleJob([this]() { idle(); })
	, myIsRunning(false)
	, myJobSequencePool(32)
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

	myJobSequences.reserve(Job_Queues_Init_Alloc);
	myWorkers.reserve(myInitInfo.myNumWorkers);

	for (uint32_t i = 0; i < info.myNumWorkers; ++i) {
		myWorkers.push_back(std::thread([this, i]() { launch_worker(i); }));
	}
}

void job_handler::reset()
{
	myIsRunning = false;

	for (size_t i = 0; i < myWorkers.size(); ++i) {
		myWorkers[i].join();
	}
	for (size_t i = 0; i < myJobSequences.size(); ++i) {
		myJobSequences[i]->reset();
		myJobSequencePool.recycle_object(myJobSequences[i]);
	}
	myJobSequences.clear();
	myJobSequences.shrink_to_fit();

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

job_sequence_impl * job_handler::create_job_sequence()
{
	job_sequence_impl* const jobSequence(myJobSequencePool.get_object());

	myJobSequences.push_back(jobSequence);

	return jobSequence;
}
void job_handler::launch_worker(uint32_t workerIndex)
{
	set_thread_name(std::string("Worker thread# " + std::to_string(workerIndex)));
	SetThreadPriority(GetCurrentThread(), myInitInfo.myWorkerPriorities);

	ourLastJobTimepoint = ourSleepTimer.now();

	myInitInfo.myOnLaunchJob();

	work();

	myInitInfo.myOnExitJob();
}
void job_handler::work()
{
	while (myIsRunning) {
		const job job(fetch_job());
		job();
	}
}
void job_handler::idle()
{
	job_handler::this_JobSequence = nullptr;

	const std::chrono::high_resolution_clock::time_point current(ourSleepTimer.now());
	const std::chrono::high_resolution_clock::time_point delta(current - ourLastJobTimepoint);

	if (std::chrono::duration_cast<std::chrono::milliseconds>(current - delta).count() < myInitInfo.mySleepThreshhold) {
		std::this_thread::yield();
	}
	else {
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
}
job job_handler::fetch_job()
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

void job_handler::set_thread_name(const std::string & name)
{
	thread_naming::THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = name.c_str();
	info.dwThreadID = GetCurrentThreadId();
	info.dwFlags = 0;

	__try {
		RaiseException(thread_naming::MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}
}