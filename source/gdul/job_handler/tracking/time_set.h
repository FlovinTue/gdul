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

#pragma once

#include <gdul/job_handler/globals.h>

#if defined(GDUL_JOB_DEBUG)
#include <gdul/job_handler/tracking/timer.h>
#include <atomic>

namespace gdul {
namespace jh_detail {

class time_set
{
public:
	time_set();
	time_set(const time_set& other);
	time_set(time_set&& other);
	time_set& operator=(time_set&& other);
	time_set& operator=(const time_set& other);

	void log_time(float completionTime);

	float get_avg() const;
	float get_max() const;
	float get_min() const;
	float get_minTimepoint() const;
	float get_maxTimepoint() const;

	std::size_t get_completion_count() const;

private:
	static timer s_globalTimer;

	std::atomic_flag m_lock;

	std::size_t m_completionCount;

	float m_totalTime;
	float m_minTime;
	float m_maxTime;
	float m_minTimepoint;
	float m_maxTimepoint;
};
}
}
#endif