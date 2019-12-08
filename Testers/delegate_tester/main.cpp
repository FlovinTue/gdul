// delegate_tester.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <gdul/WIP_delegate/delegate.h>
#include <functional>
#include <vld.h>

int main()
{
	using namespace gdul;
	uint64_t cap1(1), cap2(2), cap3(3), cap4(4), cap5(5);
	auto defaultCall = []() {};
	auto largeCall = [cap1, cap2, cap3, cap4]() {};
	auto returnCall = []() {return 1.0; };
	auto argCall = [](float f, int i) {std::cout << f << " " << i << std::endl; };
	auto largeCallArg = [cap1, cap2, cap3, cap4](float f, int i) { std::cout << f << " " << i << std::endl; };
	auto largeCallRet = [cap1, cap2, cap3, cap4]() {return 1; };
	auto smallCallRet = []() {return 1; };

	std::allocator<double> customAlloc;

	delegate<void()> one(defaultCall);
	one();
	delegate<void()> two(largeCall);
	two();
	delegate<double()> three(returnCall);
	three();
	delegate<void(float, int)> four(argCall);
	four(1.f, 1);
	delegate<void(int)> five(argCall, std::make_tuple(1.f));
	five(1);
	delegate<void()> six(argCall, std::make_tuple(1.f, 1));
	six();
	delegate<void()> seven(largeCall, customAlloc);
	seven();
	delegate<void(int)> eight(largeCallArg, std::make_tuple(1.f));
	eight(1);
	delegate<void()> nine(largeCallArg, std::make_tuple(1.f, 1));
	nine();
	delegate<void(float,int)> ten(largeCallArg);
	ten(1.f, 1);
	delegate<int()> eleven(largeCallRet);
	eleven();
	delegate<int()> twelve(smallCallRet);
	twelve();

    std::cout << "Hello World!\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
