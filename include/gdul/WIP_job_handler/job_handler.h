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
#include <gdul/WIP_job_handler/scatter_job_impl.h>
#include <gdul/WIP_job_handler/scatter_job.h>
#include <gdul/concurrent_object_pool/concurrent_object_pool.h>
#include <gdul/WIP_job_handler/chunk_allocator.h>

#pragma once

namespace gdul
{
template <class Signature>
class delegate;

namespace jh_detail
{
class job_handler_impl;

template <class T>
class scatter_job_impl;
}

class job_handler
{
public:
	job_handler();
	job_handler(const jh_detail::allocator_type& allocator);
	~job_handler();

	static thread_local job this_job;
	static thread_local worker this_worker;

	template <class T>
	scatter_job make_scatter_job(
		typename jh_detail::scatter_job_impl<T>::input_vector_type& inputOutput,
		delegate<bool(typename jh_detail::scatter_job_impl<T>::ref_value_type)>&& process,
		std::size_t batchSize);

	template <class T>
	scatter_job make_scatter_job(
		typename jh_detail::scatter_job_impl<T>::input_vector_type& input,
		typename jh_detail::scatter_job_impl<T>::output_vector_type& output,
		delegate<bool(typename jh_detail::scatter_job_impl<T>::ref_value_type)>&& process,
		std::size_t batchSize);

	worker make_worker();

	job make_job(gdul::delegate<void()>&& workUnit);

	void retire_workers();

	std::size_t num_workers() const;
	std::size_t num_enqueued() const;
private:
	concurrent_object_pool<jh_detail::scatter_job_chunk_rep, jh_detail::allocator_type>* get_scatter_job_chunk_pool();

	gdul::shared_ptr<jh_detail::job_handler_impl> m_impl;
};
template<class T>
inline scatter_job job_handler::make_scatter_job(typename jh_detail::scatter_job_impl<T>::input_vector_type & inputOutput, delegate<bool(typename jh_detail::scatter_job_impl<T>::ref_value_type)>&& process, std::size_t batchSize)
{
	jh_detail::chunk_allocator<T, jh_detail::scatter_job_chunk_rep> alloc(get_scatter_job_chunk_pool());

	shared_ptr<jh_detail::scatter_job_impl<T>> sp;
	sp = make_shared<jh_detail::scatter_job_impl<T>, decltype(alloc)>(alloc, inputOutput, std::forward<decltype(process)>(process), batchSize, this);
	return scatter_job(std::move(sp));
}
template<class T>
inline scatter_job job_handler::make_scatter_job(typename jh_detail::scatter_job_impl<T>::input_vector_type & input, typename jh_detail::scatter_job_impl<T>::output_vector_type & output, delegate<bool(typename jh_detail::scatter_job_impl<T>::ref_value_type)>&& process, std::size_t batchSize)
{
	jh_detail::chunk_allocator<T, jh_detail::scatter_job_chunk_rep> alloc(get_scatter_job_chunk_pool());

	shared_ptr<jh_detail::scatter_job_impl<T>> sp;
	sp = make_shared<jh_detail::scatter_job_impl<T>, decltype(alloc)>(alloc, input, output, std::forward<decltype(process)>(process), batchSize, this);
	return scatter_job(std::move(sp));
}
}