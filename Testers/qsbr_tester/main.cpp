


#include <gdul/WIP/qsbr.h>
#include <iostream>

int main()
{

	gdul::qsb::register_thread();
	{
		gdul::qsb::critical_section cs;
		gdul::qsb::qs_item qsCheck;

		if (gdul::qsb::update_item(qsCheck)) {
			std::cout << "Safe" << std::endl;
		}
		else {
			std::cout << "Unsafe" << std::endl;
		}
	}
	gdul::qsb::unregister_thread();
}