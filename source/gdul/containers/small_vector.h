// Copyright(c) 2021 Flovin Michaelsen
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

#pragma once

#include <gdul/memory/scratch_pad.h>
#include <gdul/utility/type_traits.h>

#include <memory>
#include <cassert>

#include <vector>

namespace gdul {

namespace sv_detail {

// Arbitrary small value
constexpr std::size_t DefaultLocalStorage = 6;
}

/// <summary>
/// std::vector-like class with built in local storage
/// </summary>
/// <typeparam name="T">Value type</typeparam>
/// <typeparam name="LocalStorage">Items in local storage. If debug iterators are enabled, they may consume some of this space</typeparam>
/// <typeparam name="Allocator">Allocator to use if capacity goes beyond local storage</typeparam>
template <class T, std::size_t LocalStorage = sv_detail::DefaultLocalStorage, class Allocator = std::allocator<T>>
class small_vector
{
	using internal_vector_type = std::vector<T, std::pmr::polymorphic_allocator<T>>;
public:
	using value_type = T;
	using size_type = std::size_t;
	using allocator_type = Allocator;
	using iterator = typename internal_vector_type::iterator;
	using const_iterator = typename internal_vector_type::const_iterator;
	using reverse_iterator = typename internal_vector_type::reverse_iterator;
	using const_reverse_iterator = typename internal_vector_type::const_reverse_iterator;

	small_vector() noexcept(false);
	explicit small_vector(const Allocator& alloc) noexcept(false);
	small_vector(size_type count, const T& value, const Allocator& alloc = Allocator());
	explicit small_vector(size_type count, const Allocator& alloc = Allocator());

	template< class InputIt >
	small_vector(InputIt first, InputIt last, const Allocator& alloc = Allocator());

	small_vector(const small_vector& other);
	small_vector(const small_vector& other, const Allocator& alloc);
	small_vector(small_vector&& other) noexcept(false);
	small_vector(small_vector&& other, const Allocator& alloc) noexcept(false);
	small_vector(std::initializer_list<T> init, const Allocator& alloc = Allocator());
	small_vector(std::vector<T, Allocator>&& vector) noexcept(false);
	small_vector(const std::vector<T, Allocator>& vector);

	operator std::vector<T, Allocator>() const;

	small_vector& operator=(const small_vector& right);
	small_vector& operator=(small_vector&& right) noexcept(false);

	void swap(small_vector& other) noexcept(false);

	allocator_type get_allocator() const noexcept;

	inline void push_back(const T& item) { m_vec.push_back(item); }
	inline void push_back(T&& item) { m_vec.push_back(std::move(item)); }

	template <class ...Args>
	inline void emplace_back(Args&& ... args) { m_vec.emplace_back(std::forward<Args...>(args)); }

	template <class ...Args>
	inline void emplace(const_iterator at, Args&& ...args) { m_vec.emplace(at, std::forward<Args...>(args)); }

	inline iterator begin() { return m_vec.begin(); }
	inline iterator end() { return m_vec.end(); }

	inline const_iterator begin() const { return m_vec.begin(); }
	inline const_iterator cbegin() const { return m_vec.cbegin(); }

	inline const_iterator end() const { return m_vec.end(); }
	inline const_iterator cend() const { return m_vec.cend(); }

	inline reverse_iterator rbegin() { return m_vec.rbegin(); }
	inline reverse_iterator rend() { return m_vec.rend(); }

	inline const_reverse_iterator rbegin() const { return m_vec.rbegin(); }
	inline const_reverse_iterator crbegin() const { return m_vec.crbegin(); }

	inline const_reverse_iterator rend() const { return m_vec.rend(); }
	inline const_reverse_iterator crend() const { return m_vec.crend(); }

	inline void reserve(size_type newCapacity) { m_vec.reserve(newCapacity); }

	inline void resize(size_type newSize) { m_vec.resize(newSize); }
	inline void resize(size_type newSize, const T& val) { m_vec.resize(newSize, val); }

	inline size_type size() const noexcept { return m_vec.size(); }

	inline void clear() noexcept { m_vec.clear(); }

	inline void shrink_to_fit() { m_vec.shrink_to_fit(); }

	template <class Iterator, std::enable_if_t<is_iterator_v<Iterator>>* = nullptr>
	inline void assign(Iterator first, Iterator last) { m_vec.assign(first, last); }
	inline void assign(std::initializer_list<T> ilist) { m_vec.assign(ilist); }
	inline void assign(const T& val, size_type newSize) { m_vec.assign(val, newSize); }

	inline const T& at(size_type index) const { return m_vec.at; }
	inline T& at(size_type index) { return m_vec.at; }

	inline size_type capacity() const noexcept { return m_vec.capacity(); }

	inline const T& back() const { return m_vec.back(); }
	inline T& back() { return m_vec.back(); }

	inline const T& front() const { return m_vec.front(); }
	inline T& front() { return m_vec.front(); }

	inline bool empty() const noexcept { return m_vec.empty(); }

	inline T& operator[](size_type pos) { return m_vec[pos]; }
	inline const T& operator[](size_type pos) const { return m_vec[pos]; }

	template <class Iterator, std::enable_if_t<is_iterator_v<Iterator>>* = nullptr>
	inline iterator insert(const_iterator at, Iterator first, Iterator last) { m_vec.insert(at, first, last); }
	inline iterator insert(const_iterator at, std::initializer_list<T> ilist) { m_vec.insert(at, ilist); }
	inline iterator insert(const_iterator at, const T& val) { m_vec.insert(at, val); }
	inline iterator insert(const_iterator at, T&& val) { return m_vec.insert(at, std::move(val)); }
	inline iterator insert(const_iterator at, size_type count, const T& val) { return m_vec.insert(at, count, val); }

	inline const void* data() const noexcept { return m_vec.data(); }
	inline void* data() noexcept { return m_vec.data(); }

	inline size_type max_size() const { return m_vec.max_size(); }

	inline iterator erase(const_iterator at) { return m_vec.erase(at); }
	inline iterator erase(const_iterator first, const_iterator last) { return m_vec.erase(first, last); }

	inline void pop_back() noexcept { m_vec.pop_back(); }

private:
	scratch_pad<LocalStorage * sizeof(T), allocator_type> m_scratchPad;
	internal_vector_type m_vec;
};
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector() noexcept(false)
	: m_scratchPad()
	, m_vec(std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
	m_vec.reserve(m_scratchPad.unused_storage() / sizeof(T));
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(const Allocator& alloc) noexcept(false)
	: m_scratchPad(alloc)
	, m_vec(std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
	m_vec.reserve(m_scratchPad.unused_storage() / sizeof(T));
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(size_type count, const T& value, const Allocator& alloc)
	: m_scratchPad(alloc)
	, m_vec(count, value, std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(size_type count, const Allocator& alloc)
	: m_scratchPad(alloc)
	, m_vec(count, std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(const small_vector& other)
	: m_scratchPad()
	, m_vec(other.m_vec, std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(const small_vector& other, const Allocator& alloc)
	: m_scratchPad(alloc)
	, m_vec(other.m_vec, std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(small_vector&& other) noexcept(false)
	: m_scratchPad()
	, m_vec(other.m_scratchPad.owns(other.m_vec.data()) ? other.m_vec : std::move(other.m_vec), std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(small_vector&& other, const Allocator& alloc) noexcept(false)
	: m_scratchPad(alloc)
	, m_vec(other.m_scratchPad.owns(other.m_vec.data()) ? other.m_vec : std::move(other.m_vec), std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(std::initializer_list<T> init, const Allocator& alloc)
	: m_scratchPad(alloc)
	, m_vec(init, std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(std::vector<T, Allocator>&& vector) noexcept(false)
	: m_scratchPad()
	, m_vec(vector.begin(), vector.end(), std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
template<class InputIt>
inline small_vector<T, LocalStorage, Allocator>::small_vector(InputIt first, InputIt last, const Allocator& alloc)
	: m_scratchPad(alloc)
	, m_vec(first, last, std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(const std::vector<T, Allocator>& vector)
	: m_vec(vector.begin(), vector.end(), std::pmr::polymorphic_allocator<T>(&m_scratchPad))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::operator std::vector<T, Allocator>() const
{
	return std::vector<T, Allocator>(m_vec.begin(), m_vec.end(), Allocator(m_scratchPad.get_parent_allocator()));
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>& small_vector<T, LocalStorage, Allocator>::operator=(const small_vector& right)
{
	m_vec = right.m_vec;
	return *this;
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>& small_vector<T, LocalStorage, Allocator>::operator=(small_vector&& right) noexcept(false)
{
	m_vec = std::move(right.m_vec);
	return *this;
}
template<class T, std::size_t LocalStorage, class Allocator>
inline void small_vector<T, LocalStorage, Allocator>::swap(small_vector& other) noexcept(false)
{
	internal_vector_type swp(std::move(other.m_vec));

	other = std::move(*this);

	m_vec = std::move(swp);
}
template<class T, std::size_t LocalStorage, class Allocator>
inline typename small_vector<T, LocalStorage, Allocator>::allocator_type small_vector<T, LocalStorage, Allocator>::get_allocator() const noexcept
{
	return m_scratchPad.get_parent_allocator();
}
}