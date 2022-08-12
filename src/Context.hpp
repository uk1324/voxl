#pragma once

#include <Allocator.hpp>
#include <iostream>

// No operator= needed for types becuase the only thing they need to manage is is their pointer registred and unregistred from the allocator.

namespace Voxl
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
	LocalObj(LocalObj& other);
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
	LocalValue(const LocalValue& other);
	~LocalValue();

	static LocalValue intNum(Int value, Context& context);
	static LocalValue floatNum(Float value, Context& context);
	static LocalValue null(Context& context);

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
LocalObj<T>::LocalObj(LocalObj& other)
	: obj(other.obj)
	, m_context(other.m_context)
{
	m_context.allocator.registerLocal(reinterpret_cast<Obj**>(&this->obj));
}

template<typename T>
LocalObj<T>::~LocalObj()
{
	m_context.allocator.unregisterLocal(reinterpret_cast<Obj**>(&this->obj));
}

class Vm;

class Context
{
public:
	Context(Value* args, int argCount, Allocator& allocator, Vm& vm);

	LocalValue args(size_t index);
	LocalValue typeErrorMustBe(std::string_view whatItMustBe);
	template<typename ...Vals>
	LocalValue call(const LocalValue& calle, Vals&&... args);
	std::optional<LocalValue> getGlobal(std::string_view name);
	void setGlobal(std::string_view name, const LocalValue& value);
	void createFunction(std::string_view name, NativeFunction function, int argCount, void* context = nullptr);

public:
	Allocator& allocator;
	Vm& vm;
//private:
public:
	Value* const m_args;
	const int m_argCount;
};

template<typename ...Vals>
LocalValue Context::call(const LocalValue& calle, Vals&&... args)
{
	if constexpr (sizeof...(args) != 0)
	{
		Value values[] = { args.value... };
		const auto top = vm.m_stack.topPtr;
		for (const auto value : values)
		{
			const auto wasPushed = vm.m_stack.push(value);
			ASSERT(wasPushed == true);
		}
		/*return LocalValue(vm.call(calle.value, values, sizeof...(args)), *this);*/
		const auto result = LocalValue(vm.call(calle.value, top, sizeof...(args)), *this);
		vm.m_stack.topPtr -= sizeof(values) / sizeof(Value);
		return result;
		// Node does the same thing as this but without variadic templates. You just pass an array.
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