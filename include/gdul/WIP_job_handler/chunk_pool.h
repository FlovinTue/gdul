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

#include <gdul/WIP_job_handler/job_handler_commons.h>

namespace gdul
{
template <class Object, class Allocator>
class concurrent_object_pool;

namespace job_handler_detail
{
template <std::size_t Size, std::size_t Align, class Allocator>
class chunk_pool
{
public:
	void* get_chunk();
	void recycle_chunk(void* chunk);

private:
	struct alignas(Align) chunk_rep
	{
		std::uint8_t m_block[Size];
	};

	concurrent_object_pool<chunk_rep, Allocator> m_pool;
};
template<std::size_t Size, std::size_t Align, class Allocator>
inline void * chunk_pool<Size, Align, Allocator>::get_chunk()
{
	return (void*)m_pool.get_object();
}
template<std::size_t Size, std::size_t Align, class Allocator>
inline void chunk_pool<Size, Align, Allocator>::recycle_chunk(void * chunk)
{
	m_pool.recycle_object((chunk_rep*)chunk);
}
}
}