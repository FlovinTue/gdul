#pragma once


namespace gdul {
namespace job_handler_detail {

template <class Callable>
class job_callable
{
public:
	job_callable(Callable&& callable);
	job_callable(const Callable& callable);

	void operator()();

private:
	Callable myCallable;
};
}
}