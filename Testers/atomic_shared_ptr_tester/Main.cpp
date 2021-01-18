
#include "pch.h"
#include <iostream>

//#define ASP_MUTEX_COMPARE

#include <memory>
#include "Tester.h"

#include <intrin.h>
#include "../Common/Timer.h"
#include <functional>
#include <assert.h>
#include <vld.h>


struct base
{
	virtual ~base() { std::cout << "destroyed base" << std::endl; }
	uint8_t memberb = '7';
};
struct derived : public base
{
	~derived() { std::cout << "destroyed derived" << std::endl; }
	uint64_t memberull = 123;
};
int main()
{
	gdul::shared_ptr<derived> d1(gdul::make_shared<derived>());
	gdul::shared_ptr<base> b1(d1);
	gdul::shared_ptr<base> b2(std::move(d1));

	gdul::shared_ptr<derived> d2(gdul::make_shared<derived>());
	gdul::shared_ptr<base> b3(nullptr);
	gdul::shared_ptr<base> b4(nullptr);
	b3 = d2;
	b4 = std::move(d2);

	gdul::shared_ptr<derived> d3(gdul::make_shared<derived>());
	gdul::shared_ptr<base> b5;
	b5 = (gdul::shared_ptr<base>)d3;

	gdul::shared_ptr<derived> d4(gdul::make_shared<derived>());
	gdul::shared_ptr<base> b6(d4);
	gdul::shared_ptr<derived> d5(static_cast<gdul::shared_ptr<derived>>(b6));


	constexpr std::size_t mksh = gdul::allocate_shared_size<int, std::allocator<int>>();
	constexpr std::size_t mkar = gdul::allocate_shared_size<int[], std::allocator<int>>(5);
	constexpr std::size_t claim = gdul::sp_claim_size<int, std::allocator<int>>();
	auto del = [](int*, std::allocator<int>) {};
	constexpr std::size_t claimdel(gdul::sp_claim_size_custom_delete<int, std::allocator<int>, decltype(del)>());

	std::allocator<int> alloc;

	gdul::shared_ptr<int> first;
	gdul::shared_ptr<int> second(nullptr);
	gdul::shared_ptr<int> third(gdul::make_shared<int>(3));
	gdul::shared_ptr<int> fourth(third);
	gdul::shared_ptr<int> fifth(std::move(fourth));
	gdul::shared_ptr<int> sixth(new int(6));

	// Removed deleter-only constructor as it did not make sense without explicit allocator arg
	//gdul::shared_ptr<int> seventh(new int(7), [](int* arg, decltype(alloc)) { delete arg; });

	gdul::shared_ptr<int> eighth(new int(8), alloc, [](int* arg, decltype(alloc) alloc) { delete arg; alloc; });
	gdul::shared_ptr<int> ninth(gdul::allocate_shared<int>(alloc, 8));

	gdul::shared_ptr<int> tenth;
	tenth = ninth;
	gdul::shared_ptr<int> eleventh;
	eleventh = std::move(ninth);

	gdul::atomic_shared_ptr<int> afirst;
	gdul::atomic_shared_ptr<int> asecond(nullptr);
	gdul::atomic_shared_ptr<int> athird(gdul::make_shared<int>(3));
	const std::size_t useCount(athird.unsafe_use_count());
	const std::size_t itemCount(athird.unsafe_item_count());
	const std::uint8_t localCount(athird.use_count_local());

	gdul::shared_ptr<int> afourthsrc(gdul::make_shared<int>(4));
	gdul::atomic_shared_ptr<int> afourth(afourthsrc);
	gdul::atomic_shared_ptr<int> afifth(std::move(afourthsrc));
	gdul::atomic_shared_ptr<int> asixth(afifth.load());
	gdul::atomic_shared_ptr<int> aseventh(asixth.unsafe_load());
	gdul::atomic_shared_ptr<int> aeighth(gdul::make_shared<int>(8));
	gdul::shared_ptr<int> aeightTarget(aeighth.exchange(gdul::make_shared<int>(88)));
	gdul::atomic_shared_ptr<int> aninth(gdul::make_shared<int>(9));
	gdul::shared_ptr<int> exch(aninth.exchange(aeightTarget));
	gdul::atomic_shared_ptr<int> atenth(gdul::make_shared<int>(10));
	gdul::shared_ptr<int> atenthexp(atenth.load());
	gdul::shared_ptr<int> atenthdes(gdul::make_shared<int>(1010));
	const bool tenres = atenth.compare_exchange_strong(atenthexp, atenthdes);

	gdul::atomic_shared_ptr<int> aeleventh(gdul::make_shared<int>(11));
	gdul::raw_ptr<int> aeleventhexp(aeleventh.get_raw_ptr());
	gdul::shared_ptr<int> aeleventhdes(gdul::make_shared<int>(1111));
	const bool eleres = aeleventh.compare_exchange_strong(aeleventhexp, aeleventhdes);

	gdul::atomic_shared_ptr<int> atwelfth(gdul::make_shared<int>(12));
	gdul::shared_ptr<int> atwelfthexp(gdul::make_shared<int>(121));
	gdul::shared_ptr<int> atwelfthdes(gdul::make_shared<int>(1212));
	const bool twelres = atwelfth.compare_exchange_strong(atwelfthexp, atwelfthdes);

	gdul::atomic_shared_ptr<int> athirteenth(gdul::make_shared<int>(13));
	gdul::raw_ptr<int> athirteenthexp(nullptr);
	gdul::shared_ptr<int> athirteenthdes(gdul::make_shared<int>(131));
	const bool thirtres = athirteenth.compare_exchange_strong(athirteenthexp, athirteenthdes);

	gdul::atomic_shared_ptr<int> aeleventhweak(gdul::make_shared<int>(11));
	gdul::raw_ptr<int> aeleventhexpweak(aeleventhweak.get_raw_ptr());
	gdul::shared_ptr<int> aeleventhdesweak(gdul::make_shared<int>(1111));
	const bool eleweakres = aeleventhweak.compare_exchange_weak(aeleventhexpweak, aeleventhdesweak);

	gdul::atomic_shared_ptr<int> atwelfthweak(gdul::make_shared<int>(12));
	gdul::shared_ptr<int> atwelfthexpweak(gdul::make_shared<int>(121));
	gdul::shared_ptr<int> atwelfthdesweak(gdul::make_shared<int>(1212));
	const bool twelweakres = atwelfthweak.compare_exchange_weak(atwelfthexpweak, atwelfthdesweak);

	gdul::atomic_shared_ptr<int> athirteenthweak(gdul::make_shared<int>(13));
	gdul::raw_ptr<int> athirteenthexpweak(nullptr);
	gdul::shared_ptr<int> athirteenthdesweak(gdul::make_shared<int>(131));
	const bool thirtweakres = athirteenthweak.compare_exchange_weak(athirteenthexpweak, athirteenthdesweak);

	athirteenth.get_raw_ptr();
	athirteenthdes.get_raw_ptr();

	gdul::shared_ptr<int> fourteenth(new int[10]{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, alloc, [](int* obj, std::allocator<std::uint8_t> /*alloc*/)
	{
		delete[] obj;
	});

	const int access1(fourteenth[0]);
	const int access5(fourteenth[4]);

	gdul::shared_ptr<int[10]> fifteenth(gdul::make_shared<int[10]>(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
	const std::size_t fifteenthCount(fifteenth.item_count());

	gdul::shared_ptr<int> sixteenth(new int, alloc);

	gdul::shared_ptr<int> seventeenth(gdul::allocate_shared<int>(std::allocator<int>(), 17));
	const std::size_t seventeenCount(seventeenth.item_count());

	struct alignas(64) over_aligned
	{
	};

	{
		gdul::shared_ptr<over_aligned> seventeen(gdul::make_shared<over_aligned>());
		seventeen = gdul::shared_ptr<over_aligned>(nullptr);
	}
	{
		std::allocator<int[]> alloc_;
		gdul::shared_ptr<int[]> eighteen(gdul::make_shared<int[]>(55));
		const std::size_t eighteenCount(eighteen.item_count());
		gdul::shared_ptr<int[]> nineteen(gdul::allocate_shared<int[]>(44444, alloc_ , 999999));
		gdul::shared_ptr<int[]> twenty(gdul::allocate_shared<int[]>(174, alloc_));
	}

	//gdul::shared_ptr<int> nulla(nullptr, std::uint8_t(5));
	gdul::raw_ptr<int> nullb(nullptr, 10);
	const std::size_t nullCount(nullb.item_count());
	gdul::atomic_shared_ptr<int> nullc(nullptr, 15);

	gdul::atomic_shared_ptr<int> tar(gdul::make_shared<int>(5));
	gdul::shared_ptr<int> des(gdul::make_shared<int>(6));

	gdul::atomic_shared_ptr<int> setver;
	setver.store(gdul::make_shared<int>(5));
	setver.unsafe_set_version(25);
	setver.unsafe_set_version(0);

	uint32_t iter(50000);
	auto lama = [&des, &tar, iter]()
	{
		for (std::uint32_t i = 0; i < iter; ++i)
		{
			gdul::raw_ptr<int> exp(nullptr);
			assert(!tar.compare_exchange_strong(exp, des));
		}
	};
	auto lamb = [&des, &tar, iter]()
	{
		for (std::uint32_t i = 0; i < iter; ++i)
		{
			gdul::raw_ptr<int> exp(nullptr);
			assert(!tar.compare_exchange_strong(exp, des));
		}
	};
	std::thread threada(lama);
	std::thread threadb(lamb);

	threada.join();
	threadb.join();

	{

		constexpr std::uint32_t testArraySize(32);
		asp_tester<std::uint64_t, testArraySize> tester(true, rand());

		const bool
			doassign(true),
			doreassign(true),
			doCAStest(true),
			doreferencetest(false),
			testAba(false);

		uint32_t arraySweeps(10000);
		uint32_t runs(10);
		float time(0.f);
		for (std::uint32_t i = 0; i < runs; ++i) {
			time += tester.execute(arraySweeps, doassign, doreassign, doCAStest, doreferencetest, testAba);
		}
		if (runs) {
#ifdef _DEBUG
			std::string config("DEBUG");
#else
			std::string config("RELEASE");
#endif
			std::string assign(doassign ? ", assign" : "");
			std::string reassign(doreassign ? ", reassign" : "");
			std::string referencetest(doreferencetest ? ", referencetest" : "");

			std::cout
				<< "Executed "
				<< runs
				<< " runs with "
				<< arraySweeps
				<< " array sweeps over "
				<< time
				<< " seconds averaging "
				<< time / runs
				<< " seconds per run in "
				<< config
				<< " mode"
				<< " using tests "
				<< assign
				<< reassign
				<< referencetest
				<< ". The number of threads used were "
				<< std::thread::hardware_concurrency()
				<< std::endl;

		}
	}
	std::cout << "Final alloc'd " << gdul::s_allocated << std::endl;
	return 0;
}
