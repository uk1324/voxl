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
	static_assert(std::is_base_of_v<Obj, T>);

	friend class LocalValue;
public:
	LocalObj(T* obj, Context& context);
	LocalObj(LocalObj& other);
	~LocalObj();
	T* operator->();
	const T* operator->() const;

public:
	T* obj;
private:
	Context& m_context;
};

class LocalObjString : public LocalObj<ObjString>
{
public:
	using LocalObj<ObjString>::LocalObj;

	std::string_view chars() const;
	size_t len() const;
	size_t size() const;
};

class LocalObjClass : public LocalObj<ObjClass>
{
public:
	using LocalObj<ObjClass>::LocalObj;
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
	LocalObjString asString();

	bool isInt() const;
	Int asInt() const;
	bool isBool() const;
	bool asBool() const;
	bool isFloat() const;
	Float asFloat() const;
	bool isNumber() const;
	Float asNumber() const;

public:
	Value value;
private:
	Context& m_context;
};

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

template<typename T>
T* LocalObj<T>::operator->()
{
	return obj;
}

template<typename T>
inline const T* LocalObj<T>::operator->() const
{
	return obj;
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
	template<typename T>
	void createClass(
		std::string_view name, 
		std::initializer_list<Allocator::Method> methods,
		InitFunction<T> init = nullptr, 
		FreeFunction<T> free = nullptr,
		void* context = nullptr);

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
			if (wasPushed == false)
			{
				[[maybe_unused]] const auto& _ = vm.fatalError("stack overflow");
				throw Vm::FatalException{};
			}
		}
		const auto result = LocalValue(vm.call(calle.value, top, sizeof...(args)), *this);
		vm.m_stack.topPtr -= sizeof(values) / sizeof(Value);
		return result;
		// Node does the same thing as this but without variadic templates. You just pass an array.
	}
	else
	{
		return LocalValue(vm.call(calle.value, nullptr, 0), *this);
	}
}

template<typename T>
void Context::createClass(
	std::string_view name, 
	std::initializer_list<Allocator::Method> methods, 
	InitFunction<T> init, 
	FreeFunction<T> free, 
	void* context)
{
	auto className = allocator.allocateStringConstant(name).value;
	auto class_ = allocator.allocateNativeClass(className, methods, vm.m_globals, init, free, context);
	vm.m_globals->set(className, Value(class_));
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
		|| (obj->asNativeInstance()->class_->mark != reinterpret_cast<MarkingFunctionPtr>(&T::mark)))
	{
		throw NativeException(m_context.typeErrorMustBe("TODO"));
	}
	return LocalObj<T>(reinterpret_cast<T*>(obj), m_context);
}

}