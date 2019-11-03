
#include "pch.h"
#include <iostream>

//#define ASP_MUTEX_COMPARE

#include "Tester.h"

#include <intrin.h>
#include "Timer.h"
#include <functional>
#include <assert.h>
#include <vld.h>

int main()
{
		using namespace gdul;

		std::allocator<uint8_t> alloc;

		shared_ptr<int> first;
		shared_ptr<int> second(nullptr);
		shared_ptr<int> third(make_shared<int>(3));
		shared_ptr<int> fourth(third);
		shared_ptr<int> fifth(std::move(fourth));
		shared_ptr<int> sixth(new int(6));
		shared_ptr<int> seventh(new int(7), [](int* arg, decltype(alloc)&) {delete arg; });	
		shared_ptr<int> eighth(new int(8), [](int* arg, decltype(alloc)& alloc) { delete arg; alloc; }, alloc);
		shared_ptr<int> ninth(make_shared<int, std::allocator<std::uint8_t>>(alloc, 8));
		shared_ptr<int> tenth;
		tenth = ninth;
		shared_ptr<int> eleventh;
		eleventh = std::move(ninth);
		
		atomic_shared_ptr<int> afirst;
		atomic_shared_ptr<int> asecond(nullptr);
		atomic_shared_ptr<int> athird(make_shared<int>(3));
		
		shared_ptr<int> afourthsrc(make_shared<int>(4));
		atomic_shared_ptr<int> afourth(afourthsrc);
		atomic_shared_ptr<int> afifth(std::move(afourthsrc));
		atomic_shared_ptr<int> asixth(afifth.load());
		atomic_shared_ptr<int> aseventh(asixth.unsafe_load());
		atomic_shared_ptr<int> aeighth(make_shared<int>(8));
		shared_ptr<int> aeightTarget(aeighth.exchange(make_shared<int>(88)));
		atomic_shared_ptr<int> aninth(make_shared<int>(9));
		aninth.exchange(aeightTarget);
		atomic_shared_ptr<int> atenth(make_shared<int>(10));
		shared_ptr<int> atenthexp(atenth.load());
		shared_ptr<int> atenthdes(make_shared<int>(1010));
		const bool tenres = atenth.compare_exchange_strong(atenthexp, atenthdes);
		
		atomic_shared_ptr<int> aeleventh(make_shared<int>(11));
		raw_ptr<int> aeleventhexp(aeleventh.get_raw_ptr());
		shared_ptr<int> aeleventhdes(make_shared<int>(1111));
		const bool eleres = aeleventh.compare_exchange_strong(aeleventhexp, aeleventhdes);
		
		atomic_shared_ptr<int> atwelfth(make_shared<int>(12));
		shared_ptr<int> atwelfthexp(make_shared<int>(121));
		shared_ptr<int> atwelfthdes(make_shared<int>(1212));
		const bool twelres = atwelfth.compare_exchange_strong(atwelfthexp, atwelfthdes);
		
		atomic_shared_ptr<int> athirteenth(make_shared<int>(13));
		raw_ptr<int> athirteenthexp(nullptr);
		shared_ptr<int> athirteenthdes(make_shared<int>(131));
		const bool thirtres = athirteenth.compare_exchange_strong(athirteenthexp, athirteenthdes);

		atomic_shared_ptr<int> aeleventhweak(make_shared<int>(11));
		raw_ptr<int> aeleventhexpweak(aeleventhweak.get_raw_ptr());
		shared_ptr<int> aeleventhdesweak(make_shared<int>(1111));
		const bool eleweakres = aeleventhweak.compare_exchange_strong(aeleventhexpweak, aeleventhdesweak);

		atomic_shared_ptr<int> atwelfthweak(make_shared<int>(12));
		shared_ptr<int> atwelfthexpweak(make_shared<int>(121));
		shared_ptr<int> atwelfthdesweak(make_shared<int>(1212));
		const bool twelweakres = atwelfthweak.compare_exchange_strong(atwelfthexpweak, atwelfthdesweak);

		atomic_shared_ptr<int> athirteenthweak(make_shared<int>(13));
		raw_ptr<int> athirteenthexpweak(nullptr);
		shared_ptr<int> athirteenthdesweak(make_shared<int>(131));
		const bool thirtweakres = athirteenthweak.compare_exchange_strong(athirteenthexpweak, athirteenthdesweak);

		athirteenth.get_raw_ptr();
		athirteenthdes.get_raw_ptr();
		
		shared_ptr<int> fourteenth(new int[10]{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, [](int* obj, std::allocator<std::uint8_t>& /*alloc*/)
		{
			delete[] obj;
		}, alloc);

		const int access1(fourteenth[0]);
		const int access5(fourteenth[4]);

		shared_ptr<int[10]> fifteenth(make_shared<int[10]>(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));

		shared_ptr<int> sixteenth(new int, alloc);

		struct alignas(64) over_aligned {
		};

		{
			shared_ptr<over_aligned> seventeen(make_shared<over_aligned>());
			seventeen = nullptr;
		}

		shared_ptr<int> nulla(nullptr, std::uint8_t(5));
		raw_ptr<int> nullb(nullptr, 10);
		atomic_shared_ptr<int> nullc(nullptr, 15);

		// Removed tagging for now. Made things complicated

		//const shared_ptr<int> preTag(athirteenth.load_and_tag());
		//const shared_ptr<int> postTag(athirteenth.load_and_tag());
		//
		//const bool preTagEval(preTag.get_tag());
		//const bool postTagEval(postTag.get_tag());

		//const int postTagStore(*athirteenth);

		atomic_shared_ptr<int> tar(make_shared<int>(5));
		shared_ptr<int> des(make_shared<int>(6));

		uint32_t iter(50000);
		auto lama = [&des, &tar, iter]() {
			for (std::uint32_t i = 0; i < iter; ++i) {
				raw_ptr<int> exp(nullptr);
				assert(!tar.compare_exchange_strong(exp, des));
			}
					};
		auto lamb = [&des, &tar, iter]() {
			for (std::uint32_t i = 0; i < iter; ++i) {
				raw_ptr<int> exp(nullptr);
				assert(!tar.compare_exchange_strong(exp, des));
			}
		};
		std::thread threada(lama);
		std::thread threadb(lamb);

		threada.join();
		threadb.join();

		const std::uint32_t testArraySize(32);
		const std::uint32_t numThreads(8);
		Tester<std::uint64_t, testArraySize, numThreads> tester(true, rand());

		const bool
			doassign(true),
			doreassign(true),
			doCAStest(true
			),
			doreferencetest(false);

		uint32_t arraySweeps(10000);
		uint32_t runs(32);
		float time(0.f);
		for (std::uint32_t i = 0; i < runs; ++i) {
			time += tester.Execute(arraySweeps, doassign, doreassign, doCAStest, doreferencetest);
		}

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
			<< numThreads
			<< std::endl;

		return 0;
}