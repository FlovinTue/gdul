#pragma once


namespace gdul {
namespace job_handler_detail {

template <class Callable>
class callable
{
public:
	callable(Callable&& callable);
	callable(const Callable& callable);

	void operator()();

private:
	Callable myCallable;
};
}
}