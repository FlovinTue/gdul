


#include "map_tester.h"
#include <iostream>
#include <vld.h>
#include <unordered_map>

int main()
{
	gdul::map_tester t;
	t.perform_speed_tests(1);
	t.perform_tests(1);
	
	std::cin.get();
}
