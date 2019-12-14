#pragma once

#include "../Common/Timer.h"
#include <vector>
#include <gdul/WIP_job_handler/scatter_job.h>

namespace gdul {

class work_tracker
{
public:
	using scatter_job_type = scatter_job<int*>;

	work_tracker(float targetTime);

	void begin_work();
	void main_work();
	void end_work();


	void begin_scatter();
	void end_scatter();

	bool scatter_process(int*& item);

private:
	void evaluate_spin_count();

	gdul::timer<float> m_workTime;
	std::size_t m_spinCount;
	const float m_targetTime;

};

}