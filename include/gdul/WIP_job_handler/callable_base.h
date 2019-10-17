#pragma once

namespace gdul {
namespace job_handler_detail {
class callable_base
{
public:
	virtual ~callable_base() = default;

	virtual void operator()() = 0;
};
}
}
