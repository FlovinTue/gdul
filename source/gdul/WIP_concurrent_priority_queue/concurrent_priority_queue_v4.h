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


template <class T, class Key>
struct rb_node
{
	Key m_key;
	T m_value;

	rb_node* m_parent;
	rb_node* m_right;
	rb_node* m_left;
};

template <class T, class Key>
class rb_tree
{
public:
	using node_type = rb_node<T, Key>;

	void insert(node_type* node);

	const node_type* find_min() const;
	const node_type* find_max() const;
	const node_type* find(Key key) const;

	node_type* delete_min();
	node_type* delete_max();
	node_type* delete_(Key key);

private:
	bool try_insert(node_type* node);

	bool claim_insertion_region(node_type* above);
	void attach_to(node_type* parent, node_type* node);

	void repair_rb_properties_insert(node_type* around);
	void repair_rb_properties_delete(node_type* around);

	node_type* find_parent() const;
};

template<class T, class Key>
inline void rb_tree<T, Key>::insert(node_type* node)
{
	while (!try_insert(node));
}

template<class T, class Key>
inline bool rb_tree<T, Key>::try_insert(node_type* node)
{
	rb_node* const parent(find_parent(node->m_key));

	if (!claim_insertion_region(parent))
		return false;

	attach_to(parent, node);

	repair_rb_properties_insert(parent);
	
	return true;
}
