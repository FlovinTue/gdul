#pragma once

#include <gdul/WIP/scratch_allocator.h>
#include <gdul/utility/type_traits.h>

#include <memory>
#include <cassert>
#include <vector>

namespace gdul {

namespace sv_detail {

// Arbitrary small value
constexpr std::size_t Default_Local_Storage = 6;
}

/// <summary>
/// Small vector class with built in local storage. If capacity goes beyond local storage, allocator will be used
/// </summary>
/// <typeparam name="T">Value type</typeparam>
/// <typeparam name="LocalStorage">Items in local storage, may slightly less than this when using debug iterators</typeparam>
/// <typeparam name="Allocator">Allocator type</typeparam>
template <class T, std::size_t LocalStorage = sv_detail::Default_Local_Storage, class Allocator = std::allocator<T>>
class small_vector
{
	using internal_vector_type = std::vector<T, scratch_allocator<T, Allocator>>;
public:
	using value_type = T;
	using size_type = std::size_t;
	using allocator_type = Allocator;
	using iterator = typename internal_vector_type::iterator;
	using const_iterator = typename internal_vector_type::const_iterator;
	using reverse_iterator = typename internal_vector_type::reverse_iterator;
	using const_reverse_iterator = typename internal_vector_type::const_reverse_iterator;

	small_vector() noexcept;
	explicit small_vector(const Allocator& alloc) noexcept;
	small_vector(size_type count, const T& value, const Allocator& alloc = Allocator());
	explicit small_vector(size_type count, const Allocator& alloc = Allocator());

	template< class InputIt >
	small_vector(InputIt first, InputIt last, const Allocator& alloc = Allocator());

	small_vector(const small_vector& other);
	small_vector(const small_vector& other, const Allocator& alloc = Allocator());
	small_vector(small_vector&& other) noexcept;
	small_vector(small_vector&& other, const Allocator& alloc);
	small_vector(std::initializer_list<T> init, const Allocator& alloc = Allocator());

	small_vector& operator=(const small_vector& right);
	small_vector& operator=(small_vector&& right);

	void swap(small_vector& other) noexcept;

	allocator_type get_allocator() const noexcept;

	__forceinline void push_back(const T& item) { m_vec.push_back(item); }
	__forceinline void push_back(T&& item) { m_vec.push_back(std::move(item)); }

	template <class ...Args>
	__forceinline void emplace_back(Args&& ... args) { m_vec.emplace_back(std::forward<Args...>(args)); }

	template <class ...Args>
	__forceinline void emplace(const_iterator at, Args&& ...args) { m_vec.emplace(at, std::forward<Args...>(args)); }

	__forceinline iterator begin() { return m_vec.begin(); }
	__forceinline iterator end() { return m_vec.end(); }

	__forceinline const_iterator begin() const { return m_vec.begin(); }
	__forceinline const_iterator cbegin() const { return m_vec.cbegin(); }

	__forceinline const_iterator end() const { return m_vec.end(); }
	__forceinline const_iterator cend() const { return m_vec.cend(); }

	__forceinline reverse_iterator rbegin() { return m_vec.rbegin(); }
	__forceinline reverse_iterator rend() { return m_vec.rend(); }

	__forceinline const_reverse_iterator rbegin() const { return m_vec.rbegin(); }
	__forceinline const_reverse_iterator crbegin() const { return m_vec.crbegin(); }

	__forceinline const_reverse_iterator rend() const { return m_vec.rend(); }
	__forceinline const_reverse_iterator crend() const { return m_vec.crend(); }

	__forceinline void reserve(size_type newCapacity) { m_vec.reserve(newCapacity); }

	__forceinline void resize(size_type newSize) { m_vec.resize(newSize); }
	__forceinline void resize(size_type newSize, const T& val) { m_vec.resize(newSize, val); }

	__forceinline size_type size() const noexcept { return m_vec.size(); }

	__forceinline void clear() noexcept { m_vec.clear(); }

	__forceinline void shrink_to_fit() { m_vec.shrink_to_fit(); }

	template <class Iterator, std::enable_if_t<is_iterator_v<Iterator>>* = nullptr>
	__forceinline void assign(Iterator first, Iterator last) { m_vec.assign(first, last); }
	__forceinline void assign(std::initializer_list<T> ilist) { m_vec.assign(ilist); }
	__forceinline void assign(const T& val, size_type newSize) { m_vec.assign(val, newSize); }

	__forceinline const T& at(size_type index) const { return m_vec.at; }
	__forceinline T& at(size_type index) { return m_vec.at; }

	__forceinline size_type capacity() const noexcept { return m_vec.capacity(); }

	__forceinline const T& back() const { return m_vec.back(); }
	__forceinline T& back() { return m_vec.back(); }

	__forceinline const T& front() const { return m_vec.front(); }
	__forceinline T& front() { return m_vec.front(); }

	__forceinline bool empty() const noexcept { return m_vec.empty(); }

	__forceinline T& operator[](size_type pos) { return m_vec[pos]; }
	__forceinline const T& operator[](size_type pos) const { return m_vec[pos]; }

	template <class Iterator, std::enable_if_t<is_iterator_v<Iterator>>* = nullptr>
	__forceinline iterator insert(const_iterator at, Iterator first, Iterator last) { m_vec.insert(at, first, last); }
	__forceinline iterator insert(const_iterator at, std::initializer_list<T> ilist) { m_vec.insert(at, ilist); }
	__forceinline iterator insert(const_iterator at, const T& val) { m_vec.insert(at, val); }
	__forceinline iterator insert(const_iterator at, T&& val) { return m_vec.insert(at, std::move(val)); }
	__forceinline iterator insert(const_iterator at, size_type count, const T& val) { return m_vec.insert(at, count, val); }

	__forceinline const void* data() const noexcept { return m_vec.data(); }
	__forceinline void* data() noexcept { return m_vec.data(); }

	__forceinline size_type max_size() const { return m_vec.max_size(); }

	__forceinline iterator erase(const_iterator at) { return m_vec.erase(at); }
	__forceinline iterator erase(const_iterator first, const_iterator last) { return m_vec.erase(first, last); }

	__forceinline void pop_back() noexcept { m_vec.pop_back(); }

private:
	scratch_pad<LocalStorage * sizeof(T)> m_scratchPad;
	internal_vector_type m_vec;
};
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector() noexcept
	: m_vec(m_scratchPad.create_allocator<T, Allocator>())
{
	m_vec.reserve(m_scratchPad.remaining_storage() / sizeof(T));
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(const Allocator& alloc) noexcept
	: m_vec(m_scratchPad.create_allocator<T, Allocator>(alloc))
{
	m_vec.reserve(m_scratchPad.remaining_storage() / sizeof(T));
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(size_type count, const T& value, const Allocator& alloc)
	: m_vec(count, value, m_scratchPad.create_allocator<T, Allocator>(alloc))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(size_type count, const Allocator& alloc)
	: m_vec(count, m_scratchPad.create_allocator<T, Allocator>(alloc))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(const small_vector& other)
	: m_vec(other, m_scratchPad.create_allocator<T, Allocator>())
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(const small_vector& other, const Allocator& alloc)
	: m_vec(other, m_scratchPad.create_allocator<T, Allocator>(alloc))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(small_vector&& other) noexcept
	: m_vec(other.m_scratchPad.owns(other.m_vec.data()) ? other : std::move(other), m_scratchPad.create_allocator<T, Allocator>())
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(small_vector&& other, const Allocator& alloc)
	: m_vec(other.m_scratchPad.owns(other.m_vec.data()) ? other : std::move(other), m_scratchPad.create_allocator<T, Allocator>(alloc))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>::small_vector(std::initializer_list<T> init, const Allocator& alloc)
	: m_vec(init, m_scratchPad.create_allocator<T, Allocator>(alloc))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
template<class InputIt>
inline small_vector<T, LocalStorage, Allocator>::small_vector(InputIt first, InputIt last, const Allocator& alloc)
	: m_vec(first, last, m_scratchPad.create_allocator<T, Allocator>(alloc))
{
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>& small_vector<T, LocalStorage, Allocator>::operator=(const small_vector& right)
{
	m_vec = right;
}
template<class T, std::size_t LocalStorage, class Allocator>
inline small_vector<T, LocalStorage, Allocator>& small_vector<T, LocalStorage, Allocator>::operator=(small_vector&& right)
{
	m_vec = std::move(right);
}
template<class T, std::size_t LocalStorage, class Allocator>
inline void small_vector<T, LocalStorage, Allocator>::swap(small_vector& other) noexcept
{
	other;
	//
}
template<class T, std::size_t LocalStorage, class Allocator>
inline typename small_vector<T, LocalStorage, Allocator>::allocator_type small_vector<T, LocalStorage, Allocator>::get_allocator() const noexcept
{
	return m_vec.get_allocator().get_parent();
}
}
