#include <Context.hpp>
#include <Vm/Vm.hpp>

using namespace Voxl;

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
	return LocalValue(Value::integer(value), context);
}

LocalValue LocalValue::floatNum(Float value, Context& context)
{
	return LocalValue(Value(value), context);
}

LocalValue LocalValue::null(Context& context)
{
	return LocalValue(Value::null(), context);
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
		throw NativeException(LocalValue::null(*this));

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
	vm.m_globals->set(nameString, Value(allocator.allocateForeignFunction(nameString, function, argCount, vm.m_globals, context)));
}
