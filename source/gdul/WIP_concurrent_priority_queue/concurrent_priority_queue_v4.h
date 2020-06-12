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
// - possible to implement with array?

// * Rb tree advantage?
// - may be used as dictionary.. map
// - may be more localized with regards to changes. Inserts will not need go through the 'back'
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


struct rb_node
{
	rb_node* m_parent;
	rb_node* m_right;
	rb_node* m_left;
};

template <class T, class Key>
class rb_tree
{
public:
	void insert(const T& in, Key key);

	const rb_node* find_min() const;
	const rb_node* find_max() const;

private:
	bool try_insert(const T& in, Key key);

	bool claim_insertion_region(rb_node* above);
	void attach_to(rb_node* parent);

	void repair_rb_properties_insert(rb_node* above);
	void repair_rb_properties_delete(rb_node* above);

	rb_node* find_parent() const;
};

template<class T, class Key>
inline void rb_tree<T, Key>::insert(const T& in, Key key)
{
	while (!try_insert(in, key));
}

template<class T, class Key>
inline bool rb_tree<T, Key>::try_insert(const T& in, Key key)
{
	rb_node* const parent(find_parent(key));

	if (!claim_insertion_region(parent))
		return false;

	rb_node* newNode(new rb_node());

	attach_to(newNode, parent);

	repair_rb_properties_insert(parent);
	
	return true;
}
