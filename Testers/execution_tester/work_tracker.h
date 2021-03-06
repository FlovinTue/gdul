#pragma once

#include <gdul/execution/job_handler/tracking/timer.h>
#include "../Common/Timer.h"
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

	timer<float> m_workTime;
	std::size_t m_spinCount;
	const float m_targetTime;

};

}