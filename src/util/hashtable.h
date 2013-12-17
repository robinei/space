#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "list.h"

template <class T, ListLink T::*LinkField, class KeyType, KeyType(T::*KeyFunc)()>
class HashTable {
public:
	typedef unsigned int size_type;
	typedef KeyType key_type;

private:
	typedef List<T, LinkField> ListType;
	typedef typename ListType::iterator IterType;

	size_type nbuckets;
	ListType *buckets;

public:
	HashTable(size_type bucket_count = 128) : nbuckets(bucket_count) {
		buckets = new ListType[nbuckets];
	}
	~HashTable() {
		delete[] buckets;
	}

	size_type bucket_count() { return nbuckets; }
	size_type bucket_size(size_type n) { return buckets[n].size(); }
	//size_type bucket(const key_type k) const {
	//}

	void insert(T *value) {
		assert(!(value->*LinkField).is_linked());
		const KeyType &key = (value->*KeyFunc)();
		size_type bucket_index = calc_hash(key) % nbuckets;
		ListType &bucket = buckets[bucket_index];

		for (IterType it = bucket.begin(); it != bucket.end(); ++it) {
			if (((*it)->*KeyFunc)() == key) {
				bucket.insert(it, value);
				bucket.erase(it);
				return;
			}
		}

		bucket.push_front(value);
	}

	T *operator[](KeyType key) {
		size_type bucket_index = calc_hash(key) % nbuckets;
		ListType &bucket = buckets[bucket_index];
		for (IterType it = bucket.begin(); it != bucket.end(); ++it)
		if (((*it)->*KeyFunc)() == key)
			return *it;
		return 0;
	}

	void rehash(size_type new_bucket_count) {
		if (nbuckets == new_bucket_count)
			return;

		ListType *old_buckets = buckets;
		size_type old_nbuckets = nbuckets;

		nbuckets = new_bucket_count;
		buckets = new ListType[nbuckets];

		for (size_type i = 0; i < old_nbuckets; ++i) {
			ListType &bucket = old_buckets[i];
			for (IterType it2, it = bucket.begin(); it != bucket.end(); it = it2) {
				T *value = *it;
				it2 = it;
				++it2;
				bucket.erase(it);
				insert(value);
			}
		}

		delete[] old_buckets;
	}

	void clear() {
		for (size_type i = 0; i < nbuckets; ++i)
			buckets[i].clear();
	}

	/*class iterator {
	List &bucket;
	typename ListType::iterator iter;


	};*/
};

#endif