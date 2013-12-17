#ifndef LISTLINK_H
#define LISTLINK_H

#include <cassert>

class ListLink {
	ListLink *prev, *next;

	// non-copyable
	ListLink(const ListLink &);
	ListLink &operator=(const ListLink &);

public:
	ListLink() : prev(0), next(0) {}
	~ListLink() { unlink(); }

	void unlink() {
		if (prev) prev->next = next;
		if (next) next->prev = prev;
		prev = 0;
		next = 0;
	}

	bool is_linked() {
		return prev != 0;
	}

private:
	ListLink(ListLink *prev, ListLink *next) : prev(prev), next(next) {}

	void insert_after(ListLink *a) {
		assert(a);
		ListLink *b = a->next;
		assert(b);
		assert(!prev);
		assert(!next);
		prev = a;
		next = b;
		a->next = this;
		b->prev = this;
	}

	void swap(ListLink &link) {
		ListLink *p = prev;
		ListLink *n = next;
		prev = link.prev;
		next = link.next;
		link.prev = p;
		link.next = n;
	}

	template <class T, ListLink T::*LinkField> friend class List;
	template <class T, ListLink T::*LinkField, class KeyType, KeyType(T::*KeyFunc)()> friend class HashTable;
};

#endif
