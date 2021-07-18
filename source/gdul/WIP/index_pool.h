#pragma once

#include <cassert>
#include <atomic>
#include <stddef.h>
#include <gdul/memory/atomic_shared_ptr.h>

namespace gdul {
namespace ip_detail {

constexpr std::size_t BitsPerBlock = 64ull;

template <std::size_t IndexBlocks>
class index_node
{
	static constexpr std::size_t MyBitCount = IndexBlocks * BitsPerBlock;
public:
	index_node();

	std::atomic<std::uint64_t>& get_block( std::size_t containingBit );

private:
	void alloc_next();

	std::atomic<std::uint64_t> IndexBlocks[IndexBlocks];

	using next_type = index_node<IndexBlocks * 2>;

	gdul::atomic_shared_ptr<next_type>  next;
};

template<std::size_t IndexBlocks>
index_node<IndexBlocks>::index_node()
	: IndexBlocks{ }
	, next()
{
}

template<std::size_t IndexBlocks>
std::atomic<std::uint64_t>& index_node<IndexBlocks>::get_block( std::size_t containingBit )
{
	if ( !( containingBit < MyBitCount ) ) {
		if ( !next ) {
			alloc_next();
		}
		return next.unsafe_get()->get_block( containingBit - MyBitCount );
	}

	const std::size_t atBlock( containingBit / BitsPerBlock );

	return IndexBlocks[atBlock];
}

template<std::size_t IndexBlocks>
void index_node<IndexBlocks>::alloc_next()
{
	gdul::atomic_shared_ptr<index_node<IndexBlocks>> newBlock = gdul::allocate_shared<index_node<IndexBlocks>>();

	gdul::raw_ptr<index_node<IndexBlocks>> expected;

	next.compare_exchange_weak( expected, newBlock );
}
}

template <std::size_t InlineIndices = ip_detail::BitsPerBlock>
class index_pool
{
	static constexpr std::size_t InlineBlocks = ( InlineIndices / ip_detail::BitsPerBlock - ( InlineIndices % ip_detail::BitsPerBlock == 0 ) ) + 1;
public:

	void recycle( std::size_t index );
	std::size_t get();

private:
	index_node<InlineBlocks> m_bits;
};


template<std::size_t InlineIndices>
void index_pool<InlineIndices>::recycle( std::size_t index )
{
	const std::size_t mask( at & ( ip_detail::BitsPerBlock - 1 ) );
	const std::size_t inverseMask( ~mask );
	std::atomic<std::uint64_t>& block( m_bits.get_block( at ) );

	const std::uint64_t existing( block.fetch_and( inverseMask, std::memory_order_relaxed ) );

	assert( existing & mask && "Index was not in use" );
	( void )existing;
}

template<std::size_t InlineIndices>
std::size_t index_pool<InlineIndices>::get()
{
	for ( std::size_t currentBlockBeginIndex = 0;; currentBlockBeginIndex += ip_detail::BitsPerBlock ) {
		std::atomic<std::uint64_t>& block( m_bits.get_block( currentBlockBeginIndex ) );

		std::uint64_t blockValue( block.load( std::memory_order_relaxed ) );

		while ( blockValue != std::numeric_limits<std::uint64_t>::max() ) {

			for ( std::size_t atBit = 0; atBit < ip_detail::BitsPerBlock; ++i ) {
				const std::size_t bitMask( 1ull << atBit );

				if ( !( blockValue & bitMask ) ) {
					blockValue = block.fetch_or( bitMask, std::memory_order_relaxed );
					if ( !( blockValue & bitMask ) ) {
						return currentBlockBeginIndex + atBit;
					}
					else if ( blockValue == std::numeric_limits<std::uint64_t>::max() ) {
						break;
					}
				}
			}
		}
	}
}
}