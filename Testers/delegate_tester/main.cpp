// delegate_tester.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <gdul/utility/delegate.h>
#include <functional>
#include <vld.h>
#include "../Common/Timer.h"
#include <functional>
#include <gdul/memory/atomic_shared_ptr.h>
#include "../Common/util.h"

uint64_t global = 0;

class cls
{
public:
	void member() { std::cout << "called member" << std::endl; }
	void member_arg(float f, int i) { std::cout << "called member with args " << f << " and " << i << std::endl; };
};

void voidFunc()
{

}

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
	delegate<void(int)> five(argCall, 1.f);
	five(1);
	delegate<void()> six(argCall, 1.f, 1);
	six();
	delegate<void()> seven(std::pair<decltype(largeCall), decltype(customAlloc)>( largeCall, customAlloc ));
	seven();
	delegate<void(int)> eight(largeCallArg, 1.f);
	eight(1);
	delegate<void()> nine(largeCallArg, 1.f, 1);
	nine();
	delegate<void(float,int)> ten(largeCallArg);
	ten(1.f, 1);
	delegate<int()> eleven(largeCallRet);
	eleven();
	delegate<int()> twelve(smallCallRet);
	twelve();

	delegate<void()> thirteen(std::pair<decltype(largeCallArg), decltype(customAlloc)>(largeCallArg, customAlloc), 1.f, 1);
	thirteen();

	cls c;
	delegate<void(cls*)> fourteen(&cls::member);
	fourteen(&c);
	delegate<void()> fifteen(&cls::member, &c);
	fifteen();
	delegate<void()> sixteen(&cls::member_arg, &c, 16.f, 116);
	sixteen();
	delegate<void(cls*, float, int)> seventeen(&cls::member_arg);
	seventeen(&c, 17.f, 177);

	delegate<void()> moveconstruct(std::move(thirteen));
	moveconstruct();
	delegate<void()> moveAssign;
	moveAssign = std::move(moveconstruct);
	moveAssign();

	delegate<void()> copyAlloc(moveAssign);
	copyAlloc();
	delegate<void()> copyAllocAssign;
	copyAllocAssign = copyAlloc;
	copyAllocAssign();

	delegate<void()> constructStatic(one);
	constructStatic();
	delegate<void()> assignStatic;
	assignStatic = constructStatic;

	delegate<void()> makeDel(make_delegate(largeCallArg, 1.f, 1));
	makeDel();
	delegate<void()> allocDel(allocate_delegate(largeCallArg, customAlloc, 1.f, 1));
	allocDel();

	struct destructible
	{
		~destructible()
		{
			std::cout << "destructible destructor ran" << std::endl;
			check = UINT32_MAX;
		}
		uint32_t check = 0;
	};

	shared_ptr<destructible> toCapture1(make_shared<destructible>());
	shared_ptr<destructible> toCapture2(make_shared<destructible>());

	delegate<void()> captureDel1([toCapture1]()
	{
		assert(toCapture1->check == 0);
	});

	delegate<void()> captureDel2([](shared_ptr<destructible> destr)
	{
		assert(destr->check == 0);
	}, toCapture2);

	toCapture1 = shared_ptr<destructible>(nullptr);
	toCapture2 = shared_ptr<destructible>(nullptr);

	captureDel1();
	captureDel2();

	auto awkwardLam = [cap1, cap2, cap3, cap4, cap5]()
	{
		global += cap1;
		global += cap2;
		global += cap3;
		global += cap4;
		global += cap5;
	};

	gdul::timer<float> time;

	for (uint32_t i = 0; i < 300000000; ++i){
		delegate<void()> constructed(awkwardLam);
	}

	const float constr(time.get());
	
	delegate<void()> something(awkwardLam);

	time.reset();
	
	for (uint32_t i = 0; i < 300000000; ++i){
		something();
	}

	const float call(time.get());
	time.reset();

	for (uint32_t i = 0; i < 300000000; ++i)
	{
		std::function<void()> constructed(awkwardLam);
	}

	const float constrf(time.get());

	std::function<void()> somethingf(awkwardLam);

	time.reset();

	for (uint32_t i = 0; i < 300000000; ++i)
	{
		somethingf();
	}

	const float callf(time.get());

	std::cout << "construct took " << constr << " seconds and call took " << call << " seconds" << std::endl;
	std::cout << "compared to std::function, which had the following times: \n";
	std::cout << "construct took " << constrf << " seconds and call took " << callf << " seconds" << std::endl;
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
