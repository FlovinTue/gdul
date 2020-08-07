#pragma once




#include <atomic>
#include <cstdint>
#include <vector>




class cpq 
{
public:


	void setup() 
	{

	}
	void push(std::uint32_t key) 
	{

	}
	bool try_pop(std::uint32_t& outKey) 
	{

	}

	std::vector<std::atomic<std::uint64_t>> m_items;
};