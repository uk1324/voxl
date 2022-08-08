#include "Context.hpp"
#include <Context.hpp>
#include <Vm/Vm.hpp>

using namespace Lang;

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

bool LocalValue::isInt() const
{
	return value.isInt();
}

Int LocalValue::asInt() const
{
	ASSERT(isInt());
	// TODO: Make asInt a function of value and the just call it from here to avoid duplication.
	return value.as.intNumber;
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
		throw NativeException(nullValue());

	return LocalValue(m_args[i], *this);
}

LocalValue Context::nullValue()
{
	return LocalValue(Value::null(), *this);
}

LocalValue Context::intValue(Int value)
{
	return LocalValue(Value::integer(value), *this);
}

LocalValue Context::typeErrorMustBe(std::string_view /*whatItMustBe*/)
{
	return LocalValue(Value::null(), *this);
}

std::optional<LocalValue> Context::getGlobal(std::string_view name)
{
	if (name == "ListIterator")
		return LocalValue(Value(vm.m_listIteratorType), *this);
	if (name == "StopIteration")
		return LocalValue(Value(vm.m_stopIterationType), *this);
	return std::nullopt;
}
