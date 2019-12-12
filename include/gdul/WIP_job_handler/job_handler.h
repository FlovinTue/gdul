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

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/worker.h>
#include <gdul/WIP_job_handler/job.h>

#pragma once

namespace gdul
{
template <class Signature>
class delegate;

template <class T>
class scatter_job;

namespace jh_detail
{
class job_handler_impl;
}

class job_handler
{
public:
	job_handler();
	job_handler(const jh_detail::allocator_type& allocator);
	~job_handler();

	void retire_workers();

	template <class T>
	shared_ptr<scatter_job<T>> make_scatter_job(
		typename scatter_job<T>::input_vector_type& inputOutput,
		delegate<bool(typename scatter_job<T>::ref_value_type)>&& process,
		std::size_t batchSize);

	template <class T>
	shared_ptr<scatter_job<T>> make_scatter_job(
		const typename scatter_job<T>::input_vector_type& input,
		typename scatter_job<T>::output_vector_type& output,
		delegate<bool(typename scatter_job<T>::ref_value_type)>&& process,
		std::size_t batchSize);

	worker make_worker();

	job make_job(gdul::delegate<void()>&& workUnit);

	std::size_t num_workers() const;
	std::size_t num_enqueued() const;
private:

	gdul::shared_ptr<jh_detail::job_handler_impl> m_impl;
	jh_detail::allocator_type m_allocator;
};
template<class T>
inline shared_ptr<scatter_job<T>> job_handler::make_scatter_job(typename scatter_job<T>::input_vector_type & inputOutput, delegate<bool(typename scatter_job<T>::ref_value_type)>&& process, std::size_t batchSize)
{
	return make_shared<scatter_job<T>, jh_detail::allocator_type>(m_allocator, inputOutput, process, batchSize, this);
}
template<class T>
inline shared_ptr<scatter_job<T>> job_handler::make_scatter_job(const typename scatter_job<T>::input_vector_type & input, typename scatter_job<T>::output_vector_type & output, delegate<bool(typename scatter_job<T>::ref_value_type)>&& process, std::size_t batchSize)
{
	return make_shared<scatter_job<T>, jh_detail::allocator_type>(m_allocator, input, output, process, batchSize, this);
}
}