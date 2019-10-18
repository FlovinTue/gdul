// Copyright(c) 2019 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <gdul\WIP_job_handler\job_impl.h>
#include <gdul\WIP_job_handler\job_handler.h>

namespace gdul
{
namespace job_handler_detail
{


job_impl::~job_impl()
{
	myCallable->~callable_base();

	if ((void*)myCallable != (void*)&myStorage[0]) {
		myAllocFields.myAllocator.deallocate(myAllocFields.myCallableBegin, myAllocFields.myAllocated);
		myAllocFields.myAllocator.~allocator_type();
	}

	std::uninitialized_fill_n(myStorage, Callable_Max_Size_No_Heap_Alloc, std::uint8_t(0));
}

void job_impl::operator()()
{
	(*myCallable)();

	myFinished.store(true, std::memory_order_seq_cst);

	enqueue_children();
}
bool job_impl::try_attach_child(job_impl_shared_ptr child)
{
	job_impl_shared_ptr firstChild(nullptr);
	job_impl_raw_ptr rawRep(nullptr);
	do {
		firstChild = myFirstChild.load();
		rawRep = firstChild;

		child->attach_sibling(std::move(firstChild));

		if (myFinished.load(std::memory_order_seq_cst)) {
			child->myFirstSibling.unsafe_store(nullptr);
			return false;
		}

	} while (!myFirstChild.compare_exchange_strong(rawRep, std::move(child)));

	return true;
}
std::uint8_t job_impl::get_priority() const
{
	return myPriority;
}
void job_impl::add_dependencies(std::uint16_t n)
{
	myDependencies.fetch_add(n, std::memory_order_relaxed);
}
std::uint16_t job_impl::remove_dependencies(std::uint16_t n)
{
	std::uint16_t result(myDependencies.fetch_sub(n, std::memory_order_acq_rel));
	return result - n;
}
bool job_impl::enable()
{
	return !remove_dependencies(Job_Max_Dependencies);
}
job_handler * job_impl::get_handler() const
{
	return myHandler;
}
bool job_impl::finished() const
{
	return myFinished.load(std::memory_order_relaxed);
}
void job_impl::attach_sibling(job_impl_shared_ptr sibling)
{
	myFirstSibling.unsafe_store(std::move(sibling));
}
void job_impl::enqueue_children()
{
	job_impl_shared_ptr child(myFirstChild.exchange(nullptr));
	if (child) {
		child->enqueue_siblings();

		if (!child->remove_dependencies(1)) {
			child->get_handler()->enqueue_job(std::move(child));
		}
	}
}
void job_impl::enqueue_siblings()
{
	job_impl_shared_ptr sibling(myFirstSibling.unsafe_load());

	while (sibling) {
		job_impl_shared_ptr next(sibling->myFirstSibling.unsafe_load());

		if (!sibling->remove_dependencies(1)) {
			sibling->get_handler()->enqueue_job(std::move(sibling));
		}

		sibling = std::move(next);
	}
}
}
}
