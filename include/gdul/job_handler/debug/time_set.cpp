// Copyright(c) 2020 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <gdul/job_handler/debug/time_set.h>

#if defined(GDUL_JOB_DEBUG)
#include <memory>
#include <thread>

namespace gdul {
namespace jh_detail {

timer time_set::s_globalTimer;

time_set::time_set()
	: m_beginTime(0.f)
	, m_totalTime(0.f)
	, m_minTime(std::numeric_limits<float>::max())
	, m_maxTime(-std::numeric_limits<float>::max())
	, m_minTimepoint(0.f)
	, m_maxTimepoint(0.f)
	, m_completionCount(0)
	, m_lock{ATOMIC_FLAG_INIT}
{}
time_set::time_set(const time_set & other)
{
	operator=(other);
}
time_set::time_set(time_set&& other)
{
	operator=(other);
}
time_set& time_set::operator=(time_set&& other)
{
	return operator=(other);
}
time_set& time_set::operator=(const time_set& other)
{
	std::memcpy(this, &other, sizeof(time_set));
	return *this;
}
void time_set::log_time(float completionTime)
{
	const float completedAt(s_globalTimer.get());

	while (m_lock.test_and_set())
		std::this_thread::yield();

	if (completionTime < m_minTime) {
		m_minTime = completionTime;
		m_minTimepoint = completedAt;
	}
	if (m_maxTime < completionTime) {
		m_maxTime = completionTime;
		m_maxTimepoint = completedAt;
	}

	m_totalTime += completionTime;

	++m_completionCount;

	m_lock.clear();
}
float time_set::get_avg() const
{
	return m_totalTime / (m_completionCount != 0 ? m_completionCount : 1);
}

float time_set::get_max() const
{
	return m_maxTime;
}

float time_set::get_min() const
{
	return m_minTime;
}

float time_set::get_minTimepoint() const
{
	return m_minTimepoint;
}

float time_set::get_maxTimepoint() const
{
	return m_maxTimepoint;
}
std::size_t time_set::get_completion_count() const
{
	return m_completionCount;
}
}
}
#endif