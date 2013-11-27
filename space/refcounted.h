#ifndef REFCOUNTED_H
#define REFCOUNTED_H

#include <boost/intrusive_ptr.hpp>

class RefCounted {
public:
	friend void intrusive_ptr_add_ref(const RefCounted *p) {
		BOOST_ASSERT(p);
		BOOST_ASSERT(p->m_refcount >= 0);
		++p->m_refcount;
	}

    friend void intrusive_ptr_release(const RefCounted *p) {
		BOOST_ASSERT(p);
		BOOST_ASSERT(p->m_refcount >= 0);
		if (--p->m_refcount == 0)
			delete const_cast<RefCounted *>(p);
	}

protected:
	RefCounted() : m_refcount(0) {}
	virtual ~RefCounted() {}

private:
    /*
    template <class T> friend class Ref;

	void addref() {
		++m_refcount;
	}

	void release() {
		if (--m_refcount <= 0)
			delete this;
	}
    */

	mutable int m_refcount;
};

/*template <class T>
class Ref {
public:
	Ref() : _pre(0) {}
	explicit Ref(T *ptr = 0) : _ptr(ptr) {
		ptr->addref();
	}
	explicit Ref(const Ref &ref) : _ptr(ref._ptr) {
		_ptr->addref();
	}

	Ref &operator=(T *ptr) {
		if (_ptr != ptr) {
			T *temp = _ptr;
			_ptr = ptr;
			if (ptr)
				ptr->addref();
			if (temp)
				temp->release();
		}
		return *this;
	}
	Ref &operator=(const Ref &ref) {
		return (*this = ref._ptr);
	}

	T *operator*() { return _ptr; }
	const T *operator*() const { return p_tr; }

	T *operator->() { return _ptr; }
	const T *operator->() const { return _ptr; }

private:
	T *_ptr;
};*/

#endif
