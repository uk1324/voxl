#include <Context.hpp>
#include <ContextTry.hpp>
#include <Vm/Vm.hpp>

using namespace Voxl;

LocalObjString::LocalObjString(std::string_view string, Context& context)
	: LocalObj(context.allocator.allocateString(string), context)
{}

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

LocalValue::LocalValue(std::string_view string, Context& context)
	: LocalValue(Value(context.allocator.allocateString(string)), context)
{}

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

LocalValue LocalValue::boolean(bool value, Context& context)
{
	return LocalValue(Value(value), context);
}

LocalValue LocalValue::null(Context& context)
{
	return LocalValue(Value::null(), context);
}

bool LocalValue::operator==(const LocalValue& other)
{
	auto& vm = m_context.vm;
	if ((vm.m_stack.push(value) == false)|| (vm.m_stack.push(other.value) == false))
	{
		TRY(vm.fatalError("stack overflow"));
	}
	TRY(vm.equals());
	return vm.m_stack.popAndReturn().asBool();
}

std::optional<LocalValue> LocalValue::at(std::string_view fieldName)
{
	const auto fieldNameString = LocalObjString(fieldName, m_context);
	const auto field = m_context.vm.atField(value, fieldNameString.obj);
	if (field.has_value())
		return LocalValue(*field, m_context);
	return std::nullopt;
}

LocalValue LocalValue::get(std::string_view fieldName)
{
	const auto fieldNameString = LocalObjString(fieldName, m_context);
	TRY(m_context.vm.getField(value, fieldNameString.obj));
	const auto result = m_context.vm.m_stack.popAndReturn();
	return LocalValue(result, m_context);
}

void LocalValue::set(std::string_view fieldName, const LocalValue& rhs)
{
	const auto fieldNameString = LocalObjString(fieldName, m_context);
	TRY(m_context.vm.setField(value, fieldNameString.obj, rhs.value));
}

LocalObjString LocalValue::asString()
{
	auto obj = value.as.obj;
	if ((value.isObj() == false) || (obj->isString() == false))
	{
		TRY(m_context.vm.throwTypeErrorExpectedFound(m_context.vm.m_stringType, value));
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

	TRY(m_context.vm.throwTypeErrorExpectedFound(m_context.vm.m_numberType, value));
	ASSERT_NOT_REACHED();
	return std::numeric_limits<Float>::infinity();
}

Context::Context(Value* args, int argCount, Allocator& allocator, Vm& vm, void* data)
	: m_args(args)
	, m_argCount(argCount)
	, allocator(allocator)
	, vm(vm)
	, data(data)
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

std::optional<LocalValue> Context::at(std::string_view name)
{
	if (const auto value = vm.m_globals->get(name); value.has_value())
		return LocalValue(*value, *this);

	if (const auto value = vm.m_builtins.get(name); value.has_value())
		return LocalValue(*value, *this);

	return std::nullopt;
}

LocalValue Context::get(std::string_view name)
{
	const auto nameString = LocalObjString(name, *this);
	const auto result = vm.getGlobal(nameString.obj);
	TRY_WITH_VALUE(result);
	return LocalValue(result.value, *this);
}

void Context::set(std::string_view name, const LocalValue& value)
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

void Context::useAllFromModule(std::string_view name)
{
	const auto nameString = allocator.allocateStringConstant(name).value;
	TRY(vm.importModule(nameString));
	auto module = vm.m_stack.top();
	module.asObj()->asModule()->isLoaded = true;
	TRY(vm.importAllFromModule(module.asObj()->asModule()));
	vm.m_stack.pop();
}

LocalValue Context::useModule(std::string_view name, std::optional<std::string_view> variableName)
{
	const auto nameString = allocator.allocateStringConstant(name).value;
	TRY(vm.importModule(nameString));
	auto module = vm.m_stack.top();
	vm.m_stack.pop();
	module.asObj()->asModule()->isLoaded = true;

	if (variableName.has_value())
	{
		const auto variableNameString = allocator.allocateStringConstant(*variableName).value;
		vm.m_globals->set(variableNameString, module);
	}
	else
	{
		vm.m_globals->set(nameString, module);
	}

	return LocalValue(module, *this);
}