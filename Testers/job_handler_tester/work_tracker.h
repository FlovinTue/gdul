#pragma once

#include "../Common/Timer.h"

namespace gdul {

class work_tracker
{
public:
	work_tracker(float targetTime);

	void begin_work();
	void main_work();
	void end_work();


private:
	void evaluate_spin_count();

	gdul::timer<float> m_workTime;
	std::size_t m_spinCount;
	const float m_targetTime;

};

}