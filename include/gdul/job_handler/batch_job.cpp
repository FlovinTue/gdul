#include "batch_job.h"
#include <gdul/job_handler/batch_job_impl.h>
#include <gdul/concurrent_object_pool/concurrent_object_pool.h>

namespace gdul {

batch_job::batch_job()
	: m_impl(nullptr)
{}
void batch_job::add_dependency(job & dependency)
{
	m_impl->add_dependency(dependency);
}
void batch_job::set_target_queue(job_queue target) noexcept
{
	m_impl->set_target_queue(target);
}
job_queue batch_job::get_target_queue() const noexcept 
{
	return m_impl->get_target_queue();
}
void batch_job::enable()
{
	m_impl->enable();
}
bool batch_job::is_finished() const noexcept
{
	return m_impl->is_finished();
}
void batch_job::wait_until_finished() noexcept
{
	m_impl->wait_until_finished();
}
void batch_job::work_until_finished(job_queue consumeFrom)
{
	m_impl->work_until_finished(consumeFrom);
}
batch_job::operator bool() const noexcept
{
	return m_impl;
}
float batch_job::get_time() const noexcept
{
	return m_impl->get_time();
}
std::size_t batch_job::get_output_size() const noexcept
{
	return m_impl->get_output_size();
}
job & batch_job::get_endjob() noexcept
{
	return m_impl->get_endjob();
}

void batch_job::set_name(const char* name)
{
	m_impl->set_name(name);
}

}
