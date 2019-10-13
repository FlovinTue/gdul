#pragma once



__declspec(novtable) class job_callable_base
{
public:
	virtual ~job_callable_base() = 0;

	virtual void operator()() = 0;

};

