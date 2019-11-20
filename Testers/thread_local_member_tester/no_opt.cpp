#include "pch.h"
#include "no_opt.h"
#include <vector>
#include <gdul/WIP_thread_local_member/thread_local_member.h>
#include "../Common/tracking_allocator.h"

#pragma optimize( "", off )
void no_opt::execute()
{
	//std::vector<gdul::shared_ptr<typename gdul::tlm_detail::index_pool<gdul::tracking_allocator<int>>::node>>& vecref(gdul::tlm_detail::index_pool<gdul::tracking_allocator<int>>::debugVec);


	//auto findFirst = [](decltype(vecref) ref)
	//{
	//	std::size_t highest(0);
	//	std::size_t index(0);
	//	for (std::size_t i = 0; i < ref.size(); ++i)
	//	{
	//		std::size_t len(0);
	//		auto raw(ref[i].get_raw_ptr());
	//		while (raw->m_next)
	//		{
	//			raw = raw->m_next.get_raw_ptr();
	//			++len;
	//		}
	//		if (highest < len)
	//		{
	//			highest = len;
	//			index = i;
	//		}
	//	}
	//	return index;
	//};

	//while (vecref.size())
	//{
	//	std::size_t i = findFirst(vecref);

	//	auto shared = vecref[i];
	//	vecref.erase(vecref.begin() + i);
	//}
}
#pragma optimize( "", on )