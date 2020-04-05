
#include "timer.h"

#if defined(GDUL_JOB_DEBUG)
namespace gdul {
namespace jh_detail {
timer::timer()
	: m_fromTime(m_clock.now())
{}
float timer::get() const
{
	return std::chrono::duration_cast<std::chrono::duration<float>>(m_clock.now() - m_fromTime).count();
}
void timer::reset()
{
	m_fromTime = m_clock.now();
}
}
}
#endif