#pragma once

#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <gdul\WIP_job_handler\job_impl.h>

namespace gdul
{
namespace job_handler_detail
{
constexpr std::size_t Job_Impl_Chunk_Size(shared_ptr<job_impl, allocator_type>::alloc_size_make_shared());

struct alignas(Job_Impl_Align) Job_Impl_Chunk_Rep { uint8_t dummy[Job_Impl_Chunk_Size]; };

template <class Dummy>
class job_impl_allocator
{
public:
	using value_type = uint8_t;

	job_impl_allocator(concurrent_object_pool<Job_Impl_Chunk_Rep, allocator_type>* chunkSrc);

	uint8_t* allocate(std::size_t);
	void deallocate(uint8_t* block, std::size_t);

private:
	concurrent_object_pool<Job_Impl_Chunk_Rep, allocator_type>* myChunkSrc;
};
template<class Dummy>
inline job_impl_allocator<Dummy>::job_impl_allocator(concurrent_object_pool<Job_Impl_Chunk_Rep, allocator_type>* chunkSrc)
	: myChunkSrc(chunkSrc)
{
}
template<class Dummy>
inline uint8_t * job_impl_allocator<Dummy>::allocate(std::size_t)
{
	return myChunkSrc->get_object();
}
template<class Dummy>
inline void job_impl_allocator<Dummy>::deallocate(uint8_t * block, std::size_t)
{
	myChunkSrc->recycle_object(block);
}
}
}