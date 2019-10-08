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

namespace std
{
	template <class T>
	class function;
}
namespace gdul {

enum class Job_layer : uint8_t
{
	// Places the job in the backmost joblayer
	back,

	// Places the job after the backmost joblayer
	next
};
class job_handler;
class job_sequence_impl;
class job_sequence
{
public:
	job_sequence(job_handler* jobHandler);
	job_sequence();
	~job_sequence();

	job_sequence(const job_sequence&) = delete;
	job_sequence& operator=(const job_sequence&) = delete;

	job_sequence(job_sequence&& other);
	job_sequence& operator=(job_sequence&& other);

	void swap(job_sequence& other);
	void swap(job_sequence&& other);

	void push(const std::function<void()>& inJob, Job_layer jobLayer);
	void push(std::function<void()>&& inJob, Job_layer jobLayer);

	void pause();
	void resume();

private:
	job_sequence_impl* myImpl;
};

}
