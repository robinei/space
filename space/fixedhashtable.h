#ifndef FIXED_HASHTABLE
#define FIXED_HASHTABLE

template <int BucketsBits, typename T, class Key>
class FixedHashTable {
private:
    enum {
        HighBits = BucketsBits,
        LowBits = 32 - HighBits,
        NumBuckets = 1 << HighBits
    };

    // random constant used in the universal hash function
    // (http://en.wikipedia.org/wiki/Universal_hashing)
    // we randomly generate this in order to make new hash functions until
    // one is found which results in low max_probe (hopefully 0)
    unsigned int hash_a : LowBits;

    // the longest probe needed to reach any value stored in this block
    unsigned int max_probe : HighBits;

    T buckets[NumBuckets];

public:
    unsigned int get_max_probe() {
        return max_probe;
    }

    unsigned int capacity() {
        return NumBuckets;
    }

    // the default was chosen by a fair dice roll. guaranteed to be random
    FixedHashTable(unsigned int hash_a = 1870964089) : hash_a(hash_a), max_probe(0) {
        clear();
    }

    void clear(unsigned int new_a = 0) {
        if (new_a != 0)
            hash_a = new_a;
        max_probe = 0;
        for (int i = 0; i < NumBuckets; ++i)
            buckets[i] = T();
    }

    bool insert(T val) {
        unsigned int h = (hash_a * Key::key(val)) >> LowBits;
        unsigned int p = 0;
        do {
            unsigned int i = (h + p) & (NumBuckets - 1);
            if (buckets[i] == T()) {
                buckets[i] = val;
                if (p > max_probe)
                    max_probe = p; // this probe was the longest yet
                return true;
            }
        } while (++p < NumBuckets);
        return false;
    }

    T lookup(unsigned int key) {
        unsigned int h = (hash_a * key) >> LowBits;
        unsigned int p = 0;
        do {
            unsigned int i = (h + p) & (NumBuckets - 1);
            T val(buckets[i]);
            if (val != T() && Key::key(val) == key)
                return val;
        } while (++p <= max_probe);
        return T();
    }

    T remove(unsigned int key) {
        unsigned int h = (hash_a * key) >> LowBits;
        unsigned int p = 0;
        do {
            unsigned int i = (h + p) & (NumBuckets - 1);
            T val(buckets[i]);
            if (val != T() && Key::key(val) == key) {
                buckets[i] = T();
                return val;
            }
        } while (++p <= max_probe);
        return T();
    }

    // rehash using randomly generated hash_a until max_probe is 0
    // or until we run out of attempts
    template <class RandFunc>
    void optimize(RandFunc &rnd, int max_attempts = 1000) {
        if (max_probe == 0)
            return; // already optimal
        
        FixedHashTable temp(*this);

        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            // rehash using new random hash_a (which must be positive and odd)
            temp.rehash(((unsigned int)rnd() & ~(unsigned int)1) + 1);

            if (temp.max_probe < max_probe) {
                *this = temp;
                if (temp.max_probe == 0)
                    break; // perfect; no point looking any more
            }
        }
    }

    void rehash(unsigned int new_a = 0) {
        FixedHashTable temp(*this);
        clear(new_a);
        for (int i = 0; i < NumBuckets; ++i)
        if (temp.buckets[i] != T())
            insert(temp.buckets[i]);
    }


    typedef T value_type;

    class iterator {
    public:
        typedef T value_type;

        T operator*() { return buckets[index]; }
        T operator->() { return buckets[index]; }

        bool operator==(iterator it) const { return index == it.index; }
        bool operator!=(iterator it) const { return index != it.index; }

        iterator &operator++() {
            if (buckets) {
                while (1) {
                    if (++index >= NumBuckets) {
                        index = -1;
                        buckets = nullptr;
                        break;
                    }
                    if (buckets[index] != T())
                        break;
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator it(*this);
            ++*this;
            return it;
        }
    private:
        friend class FixedHashTable;

        iterator(T *buckets, int index) : buckets(buckets), index(index) {
            ++*this;
        }

        T *buckets;
        int index;
    };

    iterator begin() { return iterator(buckets, -1); }
    iterator end() { return iterator(nullptr, -1); }
};

#endif
