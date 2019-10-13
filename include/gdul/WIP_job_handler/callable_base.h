#pragma once

namespace gdul {
namespace job_handler_detail {


__declspec(novtable) class callable_base
{
public:
	virtual ~callable_base() = default;

	virtual void operator()() = 0;

	virtual void dealloc() = 0;

};

}
}
