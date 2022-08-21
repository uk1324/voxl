#pragma once

#include <Allocator.hpp>
#include <ContextTry.hpp>
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
private:
	using LocalObj<ObjString>::LocalObj;
public:
	LocalObjString(std::string_view string, Context& context);

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
	LocalValue(std::string_view string, Context& context);
	~LocalValue();

	// Could implement getting fields by returing a object that could be assigned which would set the field
	// and get the field. The get field would be called on the implicit converion to LocalValue and
	// if the field doesn't exist then the an exception would be thrown. The local value could also be 
	// accessed using the '->' operator. The issues with this are that the conversion wouldn't happend when 
	// using auto. The value access might also need to be a LocalValue so it doesn't get collected.

	// If this didn't require a context argument it could use user defined literals with a _voxl prefix.
	static LocalValue intNum(Int value, Context& context);
	static LocalValue floatNum(Float value, Context& context);
	static LocalValue boolean(bool value, Context& context);
	static LocalValue null(Context& context);
	template <typename ...Vals>
	LocalValue operator()(const Vals&&... args);
	bool operator== (const LocalValue& other);
	std::optional<LocalValue> at(std::string_view fieldName);
	LocalValue get(std::string_view fieldName);
	void set(std::string_view fieldName, const LocalValue& rhs);

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
const T* LocalObj<T>::operator->() const
{
	return obj;
}

class Vm;

class Context
{
public:
	Context(Value* args, int argCount, Allocator& allocator, Vm& vm);

	LocalValue args(size_t index);
	std::optional<LocalValue> at(std::string_view name);
	LocalValue get(std::string_view name);
	void set(std::string_view name, const LocalValue& value);
	void createFunction(std::string_view name, NativeFunction function, int argCount, void* context = nullptr);
	template<typename T>
	void createClass(
		std::string_view name, 
		std::initializer_list<Allocator::Method> methods,
		InitFunction<T> init = nullptr, 
		FreeFunction<T> free = nullptr,
		void* context = nullptr);
	LocalValue useModule(std::string_view name, std::optional<std::string_view> variableName);
	void useAllFromModule(std::string_view name);

public:
	Allocator& allocator;
	Vm& vm;
//private:
public:
	Value* const m_args;
	const int m_argCount;
};

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

template<typename ...Vals>
LocalValue LocalValue::operator()(const Vals&&... args)
{
	auto& vm = m_context.vm;
	if constexpr (sizeof...(args) != 0)
	{
		Value arguments[] = { args.value... };
		TRY(vm.callAndReturnValue(value, arguments, sizeof...(args)));
		return LocalValue(vm.m_stack.popAndReturn(), m_context);
	}
	else
	{
		TRY(vm.callAndReturnValue(value));
		return LocalValue(vm.m_stack.popAndReturn(), m_context);
	}
}

template<typename T>
LocalObj<T> LocalValue::asObj()
{
	auto obj = value.as.obj;
	if ((value.isObj() == false)
		|| (obj->isNativeInstance() == false) 
		|| (obj->asNativeInstance()->isOfType<T>() == false))
	{
		TRY(m_context.vm.throwErrorWithMsg(m_context.vm.m_typeErrorType, "unexpected type"));
	}
	return LocalObj<T>(reinterpret_cast<T*>(obj), m_context);
}

}

#include <ContextTryUndef.hpp>