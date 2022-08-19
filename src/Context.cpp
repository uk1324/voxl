#include "Context.hpp"
#include <Context.hpp>
#include <Vm/Vm.hpp>

using namespace Voxl;

std::string_view LocalObjString::chars() const
{
	return std::string_view(obj->chars, obj->size);
}

size_t LocalObjString::len() const
{
	return obj->length;
}

size_t LocalObjString::size() const
{
	return obj->size;
}

LocalValue::LocalValue(const Value& value, Context& context)
	: value(value)
	, m_context(context)
{
	m_context.allocator.registerLocal(&this->value);
}

LocalValue::LocalValue(const LocalValue& other)
	: value(other.value)
	, m_context(other.m_context)
{
	m_context.allocator.registerLocal(&this->value);
}

LocalValue::~LocalValue()
{
	m_context.allocator.unregisterLocal(&value);
}

LocalValue LocalValue::intNum(Int value, Context& context)
{
	return LocalValue(Value::intNum(value), context);
}

LocalValue LocalValue::floatNum(Float value, Context& context)
{
	return LocalValue(Value(value), context);
}

LocalValue LocalValue::null(Context& context)
{
	return LocalValue(Value::null(), context);
}

LocalValue LocalValue::at(std::string_view fieldName)
{
	// Maybe change to LocalObjString. Can't be non constant because getField might allocate.
	const auto fieldNameString = m_context.allocator.allocateStringConstant(fieldName).value;
	const auto field = m_context.vm.getField(value, fieldNameString);
	if (field.has_value())
	{
		return LocalValue(*field, m_context);
	}
	// TODO: Change this.
	ASSERT_NOT_REACHED();
	throw Vm::FatalException();
}

LocalObjString LocalValue::asString()
{
	auto obj = value.as.obj;
	if ((value.isObj() == false) || (obj->isString() == false))
	{
		throw NativeException(m_context.typeErrorMustBe("TODO"));
	}
	return LocalObjString(obj->asString(), m_context);
}

bool LocalValue::isInt() const
{
	return value.isInt();
}

Int LocalValue::asInt() const
{
	ASSERT(isInt());
	return value.asInt();
}

bool LocalValue::isBool() const
{
	return value.isBool();
}

bool LocalValue::asBool() const
{
	ASSERT(isBool());
	return value.as.boolean;
}

bool LocalValue::isFloat() const
{
	return value.isFloat();
}

Float LocalValue::asFloat() const
{
	ASSERT(isFloat());
	return value.as.floatNumber;
}

bool LocalValue::isNumber() const
{
	return value.isFloat() || value.isInt();
}

Float LocalValue::asNumber() const
{
	if (value.isFloat())
		return value.asFloat();

	if (value.isInt())
		return static_cast<Float>(value.asInt());

	throw m_context.typeErrorMustBe("number");
}

Context::Context(Value* args, int argCount, Allocator& allocator, Vm& vm)
	: m_args(args)
	, m_argCount(argCount)
	, allocator(allocator)
	, vm(vm)
{}

LocalValue Context::args(size_t index)
{
	const auto i = static_cast<int>(index);

	if (i >= m_argCount)
	{
		ASSERT_NOT_REACHED();
		throw NativeException(LocalValue::null(*this));
	}

	return LocalValue(m_args[i], *this);
}

LocalValue Context::typeErrorMustBe(std::string_view /*whatItMustBe*/)
{
	return LocalValue(Value::null(), *this);
}

std::optional<LocalValue> Context::getGlobal(std::string_view name)
{
	if (const auto value = vm.m_globals->get(name); value.has_value())
		return LocalValue(*value, *this);

	if (const auto value = vm.m_builtins.get(name); value.has_value())
		return LocalValue(*value, *this);

	return std::nullopt;
}

void Context::setGlobal(std::string_view name, const LocalValue& value)
{
	vm.m_globals->set(allocator.allocateStringConstant(name).value, value.value);
}

void Context::createFunction(std::string_view name, NativeFunction function, int argCount, void* context)
{
	const auto nameString = allocator.allocateStringConstant(name).value;
	ASSERT(argCount >= 0);
	vm.m_globals->set(
		nameString, 
		Value(allocator.allocateForeignFunction(nameString, function, argCount, vm.m_globals, context)));
}

LocalValue Context::useModule(std::string_view name, std::optional<std::string_view> variableName)
{
	const auto nameString = allocator.allocateStringConstant(name).value;
	const auto result = vm.importModule(nameString);
	const auto module = vm.m_stack.top();
	vm.m_stack.pop();
	switch (result.type)
	{
	case Vm::ResultType::Exception:
		throw NativeException(result.exceptionValue);
	case Vm::ResultType::Fatal:
		throw Vm::FatalException();
	case Vm::ResultType::Ok:
		if (variableName.has_value())
		{
			const auto variableNameString = allocator.allocateStringConstant(*variableName).value;
			vm.m_globals->set(variableNameString, module);
		}
		else
		{
			vm.m_globals->set(nameString, module);
		}
	}
	return LocalValue(module, *this);
}
