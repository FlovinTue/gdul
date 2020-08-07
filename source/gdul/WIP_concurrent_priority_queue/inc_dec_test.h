#pragma once

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/thread_local_member/thread_local_member.h>

namespace gdul {
union item_pack {
	std::uint64_t payload;
	struct {
		std::uint16_t itemPadding[3];
		std::uint16_t version;
	};
};



gdul::shared_ptr<std::atomic<item_pack>[]> g_items;
std::atomic<std::uint64_t> g_size;

static constexpr std::uint64_t Tag_Mask = 1;
static constexpr std::uint64_t Version_Mask = ~(std::numeric_limits<std::uint64_t>::max() >> 16);
static constexpr std::uint64_t Pointer_Mask = ~Version_Mask & ~Tag_Mask;
static constexpr std::uint64_t Size_Mask = ~Version_Mask;

void try_repair_item(std::uint64_t desired, std::atomic<std::uint64_t>& item);
void try_repair_size(std::uint64_t& expected, gdul::shared_ptr<std::atomic<item_pack>[]>& items, std::atomic<std::uint64_t>& size);


// If versioning is used, the only way an arr[end] item would be written by another pusher is if it has a matching version to that of the size var.
void push(std::uint64_t item, gdul::shared_ptr<std::atomic<item_pack>[]>& to, std::atomic<std::uint64_t>& size) {

	item_pack sz;
	sz.payload = size.load();

	for (;;) {
		const std::size_t end(sz.payload & Size_Mask);

		item_pack expectedItem;
		expectedItem.payload = to[end].load().payload;

		if (expectedItem.version != sz.version + 1) {
			item_pack desiredItem;
			desiredItem.payload = item;
			desiredItem.version = sz.version + 1;

			if (to[end].compare_exchange_strong(expectedItem, desiredItem)) {
				expectedItem = desiredItem;
			}
		}

		if (expectedItem.version == sz.version + 1) {
			item_pack desiredSz;
			desiredSz.payload = end + 1;
			desiredSz.version = sz.version + 1;
		
			if (size.compare_exchange_strong(sz.payload, desiredSz.payload)) {
				break;
			}
		}
	}
}

bool try_pop(item_pack& outItem, gdul::shared_ptr<std::atomic<item_pack>[]>& from, std::atomic<std::uint64_t>& size) {


	item_pack sz;
	sz.payload = size.load();

	// May use fetch_sub here possibly
	for (std::size_t end(sz.payload & Size_Mask); end; end = sz.payload & Size_Mask) {

		item_pack item;
		item = from[end - 1].load();

		item_pack desiredSize;
		desiredSize.payload = end - 1;
		desiredSize.version = sz.version + 1;

		if (size.compare_exchange_strong(sz.payload, desiredSize.payload)) {
			outItem = item;
			return true;
		}
	}

	return false;
}

//void try_repair_size(std::uint64_t& expected, gdul::shared_ptr<std::atomic<std::uint64_t>[]>& items, std::atomic<std::uint64_t>& size) {
//	item_pack existing;
//	existing.item = expected;
//
//	item_pack reloadItem;
//	reloadItem.item = items[existing.size - 1].load();
//	reloadItem.size = existing.size;
//
//	if (size.compare_exchange_strong(expected, reloadItem.item))
//		expected = reloadItem.item;
//}


}




// An interesting idea is if consumers/producers could work with levels instead of array indices (conceptually. Would still be indices).
// That would mean a set of producers could fill out a level concurrently.
// If then consumers could find the untagged leaves independently and claim those items... 
// That would be nice! Somewhat scaleable!
// Increasing and decreasing level would be a hazzle, FOR SURE! :(


// The obvious and simple way to push SHOULD be:
// * Load size
// * Try write item
// * Try inc size



// Which would mean the simple pop SHOULD be:
// * Load size
// * Load item
// * Try dec size

// With tags? Using the heap mechanisms, popping means removing and entry immediately.
// Pushing, however, means tagging an index with a push struct OR pushing an item tagged.....

// It should handle like: push concurrently, however poppers need to help pushers advance away from 'back' before proceeding..
// ---------------------------------------------------------------------------------------------------------------------------
// Need a mechanism that EITHER prevents poppers from decrementing size while a push is in progress, OR
// a CAS-like retry mechanism for pushers.

// The problematic scenario is: 
//	 array [0][1], size 1
// 
// * pusher writes to index 1, stalls
// * popper claims index 0
// * pusher resumes, fails to increase size, stalls
// * other pusher successfully pushes to index 0
// * other pusher attempts to write to index 1, fails


// Or similar:
// * pusher writes to index 1, stalls
// * popper claims index 0
// * 
//
//	What to do here? 
//  In the case of help-pushing, other pusher would attempt a helping size increase.. but then pusher would still react as the push failed.
//  What if we code the loaded size version into the item..
//  This would cause the second pusher's help mechanism to fail.

// Like:
// write_item(end);
