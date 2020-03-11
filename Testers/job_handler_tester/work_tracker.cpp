#include "work_tracker.h"
#include <cmath>

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
bool work_tracker::scatter_process(int *& input, int*& output)
{
	// Just to add some heft
	for (std::size_t i = 0; i < 1000;)
	{
		double toAdd(std::sqrt(gdul::testSum));
		gdul::testSum += toAdd;
		if (gdul::testSum)
		{
			++i;
		}
	}

	if ((std::uintptr_t)input % 5 == 0) {
		output = input;
		return true;
	}
	return false;
}

bool work_tracker::scatter_process_input(int *& input)
{
	return scatter_process(input, input);
}

void work_tracker::scatter_process_update(int *& input)
{
	scatter_process_input(input);
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