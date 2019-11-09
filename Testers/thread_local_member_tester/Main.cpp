
#include "pch.h"
#include <iostream>
#include <gdul\WIP_thread_local_member\thread_local_member.h>
#include <vld.h>

int main()
{
	{
		gdul::tlm<int> aliased;
		aliased = 1;
	}
	{
		gdul::thread_local_member<std::string> first;
		gdul::thread_local_member<std::string> second;
		gdul::thread_local_member<std::string> third;
		gdul::thread_local_member<std::string> fourth;
		gdul::thread_local_member<std::string> fifth;
		first = "first john long long john brother tuck";
		second = "second john long long john brother tuck";
		third = "third john long long john brother tuck";
		fourth = "fourth john long long john brother tuck";
		fifth = "fifth john long long john brother tuck";

		std::string firstOut = first;
		std::string secondOut = second;
		std::string thirdOut = third;
		std::string fourthOut = fourth;
		std::string fifthOut = fifth;
	}
	{
		gdul::thread_local_member<int> first;
		gdul::thread_local_member<int> second;
		gdul::thread_local_member<int> third;
		gdul::thread_local_member<int> fourth;
		gdul::thread_local_member<int> fifth;

		first = 1;
		second = 2;
		third = 3;
		fourth = 4;
		fifth = 5;

		const int firstOut = first;
		const int secondOut = second;
		const int thirdOut = third;
		const int fourthOut = fourth;
		const int fifthOut = fifth;
	}

	return 0;
}