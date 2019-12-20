#include <gdul/WIP_job_handler/job.h>

namespace gdul
{
job::job()
	: m_abstr(nullptr)
	, m_storage{}
{}
job::~job()
{
	m_abstr->~job_interface(); 
	m_abstr = nullptr; 
	memset(&m_storage[0], 0, sizeof(m_storage));
}
job::job(const job & other)
{
	operator=(other);
}
job::job(job && other)
{
	operator=(std::move(other));
}
job & job::operator=(const job & other)
{
	this->~job();
	m_abstr = other.m_abstr->copy_construct_at(&m_storage[0]);
}
job & job::operator=(job && other)
{
	this->~job();
	m_abstr = other.m_abstr->move_construct_at(&m_storage[0]);
	other.m_abstr = nullptr;
	memset(&other.m_storage[0], 0, sizeof(other.m_storage));
}
void job::add_dependency(job& other)
{
	m_abstr->add_dependency(other.get_depender());
}
void job::set_priority(std::uint8_t priority) noexcept
{
	m_abstr->set_priority(priority);
}
void job::enable()
{
	m_abstr->enable();
}
bool job::is_finished() const noexcept
{
	return m_abstr->is_finished();
}
void job::wait_for_finish() noexcept
{
	m_abstr->wait_for_finish();
}
job::operator bool() const noexcept
{
	return m_abstr;
}
jh_detail::base_job_abstr & job::get_depender()
{
	return m_abstr->get_depender();
}
}