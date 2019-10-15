#pragma once

#include <gdul\WIP_job_handler\job_handler_commons.h>


namespace gdul {
namespace job_handler_detail {
GDUL_NOVTABLE class callable_base
{
public:
	virtual ~callable_base() = default;

	virtual void operator()() = 0;

	virtual void dealloc() = 0;
};
}
}
