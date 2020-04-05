#pragma once

#include <gdul/job_handler/debug/timer.h>
#include <vector>

namespace gdul {

class work_tracker
{
public:
	work_tracker(float targetTime);

	void begin_work();
	void main_work();
	void end_work();

	bool scatter_process(int*& input, int*& output);
	bool scatter_process_input(int*& input);
	void scatter_process_update(int*& input);

private:
	void evaluate_spin_count();

	gdul::jh_detail::timer m_workTime;
	std::size_t m_spinCount;
	const float m_targetTime;

};

}