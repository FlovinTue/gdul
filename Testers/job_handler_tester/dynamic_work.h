#pragma once

#include "../Common/Timer.h"

namespace gdul {

class dynamic_work
{
public:
	dynamic_work(float targetTime);

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