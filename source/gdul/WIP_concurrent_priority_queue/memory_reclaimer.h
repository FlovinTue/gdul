#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <stdint.h>
#include <gdul/thread_local_member/thread_local_member.h>




namespace gdul {



class memory_reclaimer
{
public:

	memory_reclaimer()
		:m_nextIndexSlot(0)
		, m_indices(16)
		, t_container()
	{
	}


private:
	struct alignas(std::hardware_destructive_interference_size) actor_index { std::size_t i; };

	std::atomic_uint m_nextIndexSlot;
	std::vector<actor_index> m_indices;


	struct tl_container
	{
		std::uint8_t indexSlot;
	};


	tlm<tl_container> t_container;
};









}