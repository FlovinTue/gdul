
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
	, myPriorityIndexIterator(0)
	, myLastJobPriorityIndex(1)
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
	myPriorityIndexIterator.store(0, std::memory_order_relaxed);
	myLastJobPriorityIndex.store(1, std::memory_order_relaxed);
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
	uint64_t priorityIndex(0);
	if (jobPlacement == Job_layer::next) {
		priorityIndex = (myPriorityIndexIterator.fetch_add(1, std::memory_order_relaxed) + 1);
	}
	else {
		priorityIndex = myLastJobPriorityIndex.load(std::memory_order_acquire);
	}
	myJobQueue.push(inJob, priorityIndex);
}

void job_sequence_impl::push(std::function<void()>&& inJob, Job_layer jobPlacement)
{
	uint64_t priorityIndex(0);
	if (jobPlacement == Job_layer::next) {
		priorityIndex = (myPriorityIndexIterator.fetch_add(1, std::memory_order_relaxed) + 1);
	}
	else {
		priorityIndex = myPriorityIndexIterator.load(std::memory_order_acquire);
	}

	myJobQueue.push(std::move(inJob), priorityIndex);
}

bool job_sequence_impl::fetch_job(job& aOut)
{
	if (myIsPaused.load(std::memory_order_acquire)) {
		return false;
	}
	if (!myJobQueue.size()) {
		return false;
	}

	const uint64_t lastPriorityIndex(myLastJobPriorityIndex.load(std::memory_order_acquire));

	uint64_t topPriorityIndex(0);
	const bool foundKey(myJobQueue.try_peek_top_key(topPriorityIndex));
	const bool differingIndices(topPriorityIndex ^ lastPriorityIndex);
	if (foundKey && differingIndices) {
		return false;
	}

	myNumActiveJobs.fetch_add(1, std::memory_order_acq_rel);

	std::function<void()> out;
	uint64_t expectedKey(lastPriorityIndex);
	if (myJobQueue.compare_try_pop(out, expectedKey)) {
		aOut = job(std::move(out));
		return true;
	}

	advance();

	return false;
}
void job_sequence_impl::advance()
{
	const uint64_t lastPriorityIndex(myLastJobPriorityIndex.load(std::memory_order_acquire));

	uint64_t topPriorityIndex(0);
	const bool foundKey(myJobQueue.try_peek_top_key(topPriorityIndex));
	const bool oldPriorityIndex(lastPriorityIndex == topPriorityIndex);
	const bool foundNextIndex(foundKey & !oldPriorityIndex);

	const bool hasActiveJobs(myNumActiveJobs.fetch_sub(1, std::memory_order_acq_rel) - 1);

	if (foundNextIndex & !hasActiveJobs) {
		uint64_t expected(lastPriorityIndex);
		const uint64_t desired(topPriorityIndex);

		const job_priority_comparator comparator;
		do {
			if (myLastJobPriorityIndex.compare_exchange_strong(expected, desired, std::memory_order_acquire, std::memory_order_release)) {
				break;
			}
		} while (comparator(expected, desired));
	}
}
}