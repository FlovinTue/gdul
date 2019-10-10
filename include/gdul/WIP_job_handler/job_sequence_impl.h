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

#include <..\Testers\job_handler_tester\concurrent_heap.h>
#include <atomic>
#include <functional>

namespace gdul {

struct job_priority_comparator
{
	constexpr bool operator()(uint64_t aFirst, uint64_t aSecond) const;
};

enum class Job_layer : uint8_t;
class job;
class job_sequence_impl
{
public:
	job_sequence_impl();
	~job_sequence_impl();

	job_sequence_impl(const job_sequence_impl&) = delete;
	job_sequence_impl& operator=(const job_sequence_impl&) = delete;

	job_sequence_impl(job_sequence_impl&&) = delete;
	job_sequence_impl& operator=(job_sequence_impl&&) = delete;

	void run_synchronous(job&& job);
	void run_synchronous(const job& job);

	void run_asynchronous(job&& job);
	void run_asynchronous(const job& job);

	void reset();

	void pause();
	void resume();

	bool fetch_job(job& aOut);

	void advance(uint64_t fromPriorityKey);

	bool active() const;

	void  return_to_pool();

private:
	concurrent_heap<job, job_priority_comparator> myJobQueue;

	job_handler* const myHandler;

	std::atomic<uint64_t> mySequenceKeyIterator;
	std::atomic<uint64_t> myLastJobSequenceKey;
	std::atomic<uint16_t> myNumActiveJobs;
	std::atomic<bool> myIsPaused;


};

}
