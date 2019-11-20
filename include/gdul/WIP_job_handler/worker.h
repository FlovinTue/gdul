// Copyright(c) 2019 Flovin Michaelsen
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

#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul
{
namespace job_handler_detail
{
class worker_impl;
}
class worker
{
public:
	worker(job_handler_detail::worker_impl* impl);
	worker(const worker&) = default;
	worker(worker&&) = default;
	worker& operator=(const worker&) = default;
	worker& operator=(worker&&) = default;

	~worker();

	// Sets core affinity. job_handler_detail::Worker_Auto_Affinity represents automatic setting
	void set_core_affinity(std::uint8_t core = job_handler_detail::Worker_Auto_Affinity);

	// Sets which job queue to consume from. job_handler_detail::Worker_Auto_Affinity represents
	// dynamic runtime selection
	void set_queue_affinity(std::uint8_t queue = job_handler_detail::Worker_Auto_Affinity);

	// Thread priority as defined in WinBase.h
	void set_execution_priority(std::int32_t priority);

	// Thread name
	void set_name(const char* name);

	void activate();
	bool deactivate();

	bool is_active() const;

private:
	friend class job_handler;

	job_handler_detail::worker_impl* m_impl;
};
}