


#include <gdul/WIP/qsbr.h>
#include <iostream>

int main()
{

	gdul::qsbr::register_thread();
	{
		gdul::qsbr::critical_section cs;
		gdul::qsbr::item qsCheck;

		gdul::qsbr::reset(qsCheck);
		gdul::qsbr::set(qsCheck);

		if (gdul::qsbr::update(qsCheck)) {
			std::cout << "Safe" << std::endl;
		}
		else {
			std::cout << "Unsafe" << std::endl;
		}
	}
	gdul::qsbr::unregister_thread();
}