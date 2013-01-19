#ifndef CALCULON_ALLOCATOR_H
#define CALCULON_ALLOCATOR_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class Allocator;

class Object
{
private:
	Object(const Object&);
	Object* operator = (const Object&);

public:
	Object()
	{
	}

	virtual ~Object()
	{
	}
};

class Allocator
{
	typedef set<Object*> Objects;
	Objects _objects;

public:
	Allocator()
	{
	}

	~Allocator()
	{
		for (Objects::const_iterator i = _objects.begin(),
				e = _objects.end(); i != e; i++)
		{
			delete *i;
		}
	}

	template <class T>
	T* retain(T* object)
	{
		auto_ptr<T> ptr(object); // make exception safe
		_objects.insert(object); // remember pointer
		ptr.release();
		return object;
	}
};

#endif
