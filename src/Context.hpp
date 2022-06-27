#pragma once

#include <Allocator.hpp>

namespace Lang
{

class Context;
class LocalValue;

template<typename T>
class LocalObj
{
	friend class LocalValue;
public:
	T* operator->();
	LocalObj(T* obj, Context& context);
	~LocalObj();

public:
	T* obj;
private:
	Context& m_context;
};

class LocalValue
{
public:
	LocalValue(const Value& value, Context& context);
	template<typename T>
	LocalValue(const LocalObj<T>& obj);
	~LocalValue();

	template<typename T>
	LocalObj<T> asObj();

	bool isInt() const;
	Int asInt() const;
	bool isBool() const;
	bool asBool() const;
	bool isFloat() const;
	Float asFloat() const;

public:
	Value value;
private:
	Context& m_context;
};

template<typename T>
T* LocalObj<T>::operator->()
{
	return obj;
}

template<typename T>
LocalObj<T>::LocalObj(T* obj, Context& context)
	: obj(obj)
	, m_context(context)
{
	m_context.allocator.registerLocal(reinterpret_cast<Obj**>(&this->obj));
}

template<typename T>
LocalObj<T>::~LocalObj()
{
	m_context.allocator.unregisterLocal(reinterpret_cast<Obj**>(&obj));
}

class Vm;

class Context
{
public:
	Context(Value* args, int argCount, Allocator& allocator, Vm& vm);

	LocalValue args(size_t index);
	LocalValue nullValue();
	LocalValue intValue(Int value);
	LocalValue typeErrorMustBe(std::string_view whatItMustBe);
	template<typename ...Vals>
	LocalValue call(LocalValue calle, Vals... args);
	std::optional<LocalValue> getGlobal(std::string_view name);

public:
	Allocator& allocator;
	Vm& vm;
private:
	Value* const m_args;
	const int m_argCount;
};

template<typename ...Vals>
LocalValue Context::call(LocalValue calle, Vals... args)
{
	if constexpr (sizeof...(args) != 0)
	{
		Value values[] = { args.value... };
		return LocalValue(vm.call(calle.value, values, sizeof...(args)), *this);
		// Node does the same thing as this but without variadic templates.
	}
	else
	{
		return LocalValue(vm.call(calle.value, nullptr, 0), *this);
	}
	// TODO: Maybe pushing the values onto the stack and calling them would be faster. 
}

template<typename T>
LocalValue::LocalValue(const LocalObj<T>& obj)
	: LocalValue(Value(reinterpret_cast<Obj*>(obj.obj)), obj.m_context)
{}

template<typename T>
LocalObj<T> LocalValue::asObj()
{
	auto obj = value.as.obj;
	if ((value.isObj() == false)
		|| (obj->isNativeInstance() == false) 
		|| (obj->asNativeInstance()->class_->mark != reinterpret_cast<MarkingFunction>(&T::mark)))
	{
		throw NativeException(m_context.typeErrorMustBe("TODO"));
	}
	return LocalObj<T>(reinterpret_cast<T*>(obj), m_context);
}

}