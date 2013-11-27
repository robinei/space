#ifndef WEAKREF_H
#define WEAKREF_H


class WeakLink {
public:
	WeakLink() : _backptr(nullptr) {}

	~WeakLink() {
		unlink();
	}

	void unlink() {
		if (_backptr) {
			*_backptr = nullptr;
			_backptr = nullptr;
		}
	}

	bool is_linked() {
		return _backptr != nullptr;
	}

private:
	template <class T, WeakLink T::*LinkField> friend class WeakRef;
	void **_backptr;
};


template <class T, WeakLink T::*LinkField>
class WeakRef {
public:
	explicit WeakRef() : _ptr(nullptr) {
	}
	explicit WeakRef(T *ptr) : _ptr(ptr) {
		if (_ptr) {
			assert(!(_ptr->*LinkField)._backptr);
			(_ptr->*LinkField)._backptr = (void **)&_ptr;
		}
	}
	~WeakRef() {
		if (_ptr)
			(_ptr->*LinkField)._backptr = (void **)nullptr;
	}

	void reset(T *ptr = nullptr) {
		if (_ptr)
			(_ptr->*LinkField)._backptr = (void **)nullptr;
		_ptr = ptr;
		if (_ptr) {
			assert(!(_ptr->*LinkField)._backptr);
			(_ptr->*LinkField)._backptr = (void **)&_ptr;
		}
	}

	T *operator*() { return _ptr; }
	const T *operator*() const { return _ptr; }

	T *operator->() { return _ptr; }
	const T *operator->() const { return _ptr; }

	T *get() { return _ptr; }
	const T *get() const { return _ptr; }

	operator bool() const { return _ptr != nullptr; }

	bool operator==(T *ptr) const { return _ptr == ptr; }
	bool operator==(const WeakRef &ref) const { return _ptr == ref._ptr; }

	bool operator!=(T *ptr) const { return _ptr != ptr; }
	bool operator!=(const WeakRef &ref) const { return _ptr != ref._ptr; }

private:
	WeakRef(WeakRef &);
	WeakRef &operator=(WeakRef &);

	T *_ptr;
};

#endif
