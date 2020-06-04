#pragma once


// * Node based design
// * Heap?
// * Local locking with helper mechanism
// - a set of nodes together form the scope of the operation
// - the nodes within the scope needs to be linked to the operation
// - the operation must explicitly or implicitly specify *all* the necessary steps to completion
// * Separate helper structure, tracking the completion?
// * Heap advantage over rb tree?
// - much simpler
// * Rb tree advantage?
// - may be used as dictionary.. map
// - may be more localized with regards to changes
// - may not have as high contention to root



// A common scenario would be:

//					A
//				   / \
//				  /	  \
//				 B	   C
//				/ \	  / \
//			   D   E F	 G
//
// 7 nodes.
// in the intended scenario we're always working towards an empty tree
// in THIS scenario we want to limit collisions. (We're going for the optimal solution in few-item scenarios)