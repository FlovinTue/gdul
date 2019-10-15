#pragma once

#include <gdul\WIP_job_handler\job_handler_commons.h>
#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\WIP_job_handler\job_impl.h>

namespace gdul
{
namespace job_handler_detail
{


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