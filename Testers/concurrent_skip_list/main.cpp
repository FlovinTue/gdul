


#include "map_tester.h"
#include <iostream>
#include <unordered_map>

int main()
{
	gdul::map_tester t;
	t.perform_tests(2000);

	std::cin.get();

	std::unordered_map<int, int> blah;
}
