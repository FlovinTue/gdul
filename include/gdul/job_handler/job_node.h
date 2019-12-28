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

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

namespace gdul
{
namespace jh_detail
{
template <class T, class ChunkRep>
class chunk_allocator;

class job_impl;

struct job_node
{
	gdul::shared_ptr<job_node> m_next;
	gdul::shared_ptr<job_impl> m_job;
};

// Memory chunk representation of job_node
struct alignas(alignof(job_node)) job_node_chunk_rep
{
	std::uint8_t dummy[alloc_size_make_shared<job_node, chunk_allocator<jh_detail::job_node, job_node_chunk_rep>>()]{};
};
using job_node_shared_ptr = gdul::shared_ptr<job_node>;
using job_node_atomic_shared_ptr = gdul::atomic_shared_ptr<job_node>;
using job_node_raw_ptr = gdul::raw_ptr<job_node>;
}
}