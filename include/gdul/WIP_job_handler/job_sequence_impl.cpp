
#include <gdul\WIP_job_handler\job_sequence_impl.h>
#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\WIP_job_handler\job.h>

namespace std
{
template <class _Ty>
class function;
}

namespace gdul
{
constexpr bool job_priority_comparator::operator()(uint64_t aFirst, uint64_t aSecond) const
{
	const uint64_t delta(aSecond - aFirst);
	const uint64_t underflowBarrier(std::numeric_limits<uint64_t>::max() / 2);

	const bool noUnderflow(delta < underflowBarrier);
	const bool noEq(delta);
	const bool returnValue(noEq & noUnderflow);

	return returnValue;
}

job_sequence_impl::job_sequence_impl()
	: myJobQueue(job_handler::Job_Queues_Init_Alloc)
	, mySequenceKeyIterator(0)
	, myLastJobSequenceKey(1)
	, myIsPaused(false)
	, myNumActiveJobs(0)
{
}
job_sequence_impl::~job_sequence_impl()
{
}
void job_sequence_impl::reset()
{
	myJobQueue.clear();
	myJobQueue.shrink_to_fit();
	myJobQueue.reserve(job_handler::Job_Queues_Init_Alloc);
	mySequenceKeyIterator.store(0, std::memory_order_relaxed);
	myLastJobSequenceKey.store(1, std::memory_order_relaxed);
	myIsPaused.store(false, std::memory_order_relaxed);
	myNumActiveJobs.store(0, std::memory_order_relaxed);

	std::atomic_thread_fence(std::memory_order_release);
}

void job_sequence_impl::pause()
{
	myIsPaused.store(true, std::memory_order_release);
}
void job_sequence_impl::resume()
{
	myIsPaused.store(false, std::memory_order_release);
}
void job_sequence_impl::push(const std::function<void()>& inJob, Job_layer jobPlacement)
{
	uint64_t SequenceKey(0);
	if (jobPlacement == Job_layer::next) {
		SequenceKey = (mySequenceKeyIterator.fetch_add(1, std::memory_order_relaxed) + 1);
	}
	else {
		SequenceKey = myLastJobSequenceKey.load(std::memory_order_acquire);
	}
	myJobQueue.push(inJob, SequenceKey);
}

void job_sequence_impl::push(std::function<void()>&& inJob, Job_layer jobPlacement)
{
	uint64_t SequenceKey(0);
	if (jobPlacement == Job_layer::next) {
		SequenceKey = (mySequenceKeyIterator.fetch_add(1, std::memory_order_relaxed) + 1);
	}
	else {
		SequenceKey = mySequenceKeyIterator.load(std::memory_order_acquire);
	}

	myJobQueue.push(std::move(inJob), SequenceKey);
}

bool job_sequence_impl::fetch_job(job& out)
{
	if (myIsPaused.load(std::memory_order_acquire)) {
		return false;
	}
	if (!myJobQueue.size()) {
		return false;
	}

	const uint64_t lastSequenceKey(myLastJobSequenceKey.load(std::memory_order_acquire));

	uint64_t topSequenceKey(0);
	const bool foundKey(myJobQueue.try_peek_top_key(topSequenceKey));
	const bool differingIndices(topSequenceKey ^ lastSequenceKey);
	if (foundKey && differingIndices) {
		return false;
	}

	myNumActiveJobs.fetch_add(1, std::memory_order_acq_rel);

	uint64_t expectedKey(lastSequenceKey);
	if (myJobQueue.compare_try_pop(out, expectedKey)) {
		return true;
	}

	advance(lastSequenceKey);

	return false;
}
void job_sequence_impl::advance(uint64_t fromSequenceKey)
{
	uint64_t topSequenceKey(0);
	const bool foundKey(myJobQueue.try_peek_top_key(topSequenceKey));
	const bool oldSequenceKey(fromSequenceKey == topSequenceKey);
	const bool foundNextKey(foundKey & !oldSequenceKey);

	const bool hasActiveJobs(myNumActiveJobs.fetch_sub(1, std::memory_order_acq_rel) - 1);

	if (foundNextKey & !hasActiveJobs) {
		uint64_t expected(fromSequenceKey);
		const uint64_t desired(topSequenceKey);

		const job_priority_comparator comparator;
		do {
			if (myLastJobSequenceKey.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_acquire)) {
				break;
			}
		} while (comparator(expected, desired));
	}
}
bool job_sequence_impl::active() const
{
	const bool enqueued(myJobQueue.size());
	const bool active(myNumActiveJobs.load(std::memory_order_acquire));
	return enqueued | active;
}
void job_sequence_impl::return_to_pool()
{
	const job recycler([this]() {
		this->myHandler->recycle_job_sequence(this);
	});

	run_synchronous(recycler);
}
}