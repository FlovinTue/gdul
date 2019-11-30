#include "work_tracker.h"

namespace gdul {
thread_local double testSum = static_cast<double>(rand() % 100);


work_tracker::work_tracker(float targetTime)
	: m_targetTime(targetTime)
	, m_spinCount(100) // arbitrary initial value
{
}

void work_tracker::begin_work()
{
	m_workTime.reset();
}

void work_tracker::main_work()
{
	std::size_t count(m_spinCount);

	for (std::size_t i = 0; i < count;)
	{
		double toAdd(std::sqrt(gdul::testSum));
		gdul::testSum += toAdd;
		if (gdul::testSum)
		{
			++i;
		}
	}
}

void work_tracker::end_work()
{
	evaluate_spin_count();
}

void work_tracker::evaluate_spin_count()
{
	const float time(m_workTime.get());
	const float fraction(m_targetTime / time);
	const float spinCount((float)m_spinCount);
	const float newSpinCount(spinCount * fraction);

	m_spinCount = (std::size_t)newSpinCount;
}
}