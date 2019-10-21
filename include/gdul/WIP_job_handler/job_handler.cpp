

#include <gdul\WIP_job_handler\job_handler.h>
#include <string>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gdul
{

#undef GetObject
#undef max

thread_local job job_handler::this_job(nullptr);
thread_local worker job_handler::this_worker(&job_handler::ourImplicitWorker);
thread_local job_handler_detail::worker_impl* job_handler::this_worker_impl(&job_handler::ourImplicitWorker);
thread_local job_handler_detail::worker_impl job_handler::ourImplicitWorker;

job_handler::job_handler()
	: myJobImplChunkPool(job_handler_detail::Job_Impl_Allocator_Block_Size, myMainAllocator)
	, myJobImplAllocator(&myJobImplChunkPool)
	, myWorkerCount(0)
{
}

job_handler::job_handler(allocator_type & allocator)
	: myMainAllocator(allocator)
	, myJobImplChunkPool(job_handler_detail::Job_Impl_Allocator_Block_Size, myMainAllocator)
	, myJobImplAllocator(&myJobImplChunkPool)
	, myWorkerCount(0)
{
}


job_handler::~job_handler()
{
	retire_workers();
}

void job_handler::retire_workers()
{
	const std::uint16_t workers(myWorkerCount.exchange(0, std::memory_order_seq_cst));

	for (size_t i = 0; i < workers; ++i) {
		myWorkers[i].retire();
	}
}

worker job_handler::create_worker()
{
	const std::uint16_t index(myWorkerCount.fetch_add(1, std::memory_order_relaxed));
	const std::uint8_t coreAffinity(static_cast<std::uint8_t>(index % std::thread::hardware_concurrency()));

	worker_impl impl(std::thread(&job_handler::launch_worker, this, index), coreAffinity);

	myWorkers[index] = std::move(impl);

	return worker(&myWorkers[index]);
}

void job_handler::enqueue_job(job_impl_shared_ptr job)
{
	const std::uint8_t priority(job->get_priority());

	myJobQueues[priority].push(std::move(job));
}

void job_handler::launch_worker(std::uint16_t index)
{
	this_worker_impl = &myWorkers[index];
	this_worker = worker(this_worker_impl);
	this_worker_impl->refresh_sleep_timer();

	//myInitInfo.myOnThreadLaunch();

	work();

	//myInitInfo.myOnThreadExit();
}
void job_handler::work()
{
	while (!this_worker_impl->is_retired()) {

		this_job = job(fetch_job());

		if (this_job.myImpl) {
			(*this_job.myImpl)();

			this_worker_impl->refresh_sleep_timer();

			continue;
		}

		this_worker_impl->idle();
	}
}

job_handler::job_impl_shared_ptr job_handler::fetch_job()
{
	const uint8_t queueIndex(this_worker_impl->get_queue_affinity());

	job_handler::job_impl_shared_ptr out(nullptr);

	for (uint8_t i = 0; i < job_handler_detail::Priority_Granularity; ++i) {

		const uint8_t index((queueIndex + i) % job_handler_detail::Priority_Granularity);

		if (myJobQueues[index].try_pop(out)) {
			return out;
		}
	}

	return out;
}


}