#include <Json/JsonValue.hpp>
#include <Utils/Assertions.hpp>

Json::Value::Value()
	: m_type(Type::Null)
	, m_value(nullptr)
{
	m_value.null = nullptr;
}

Json::Value Json::Value::emptyObject()
{
	return Value(std::initializer_list<std::pair<const StringType, Value>>());
}

Json::Value Json::Value::emptyArray()
{
	return Value(std::initializer_list<IntType>());
}

Json::Value Json::Value::null()
{
	return Json::Value(nullptr);
}

Json::Value::Value(const StringType& string)
	: m_type(Type::String)
	, m_value(string)
{}

Json::Value::Value(StringType&& string) noexcept
	: m_type(Type::String)
	, m_value(std::move(string))
{}

// const char* implicitly gets converted to bool without this
Json::Value::Value(const char* string)
	: m_type(Type::String)
	, m_value(StringType(string))
{}

Json::Value::Value(FloatType number)
	: m_type(Type::Float)
	, m_value(number)
{}

Json::Value::Value(IntType number)
	: m_type(Type::Int)
	, m_value(number)
{}

Json::Value::Value(bool boolean)
	: m_type(Type::Boolean)
	, m_value(boolean)
{}

Json::Value::Value(std::nullptr_t)
	: m_type(Type::Null)
	, m_value(nullptr)
{}

Json::Value::Value(std::initializer_list<std::pair<const StringType, Value>> object)
	: m_type(Type::Object)
	, m_value(MapType(object))
{}

Json::Value::Value(std::initializer_list<FloatType> array)
	: m_type(Type::Array)
{
	setArray(array);
}

Json::Value::Value(std::initializer_list<IntType> array)
	: m_type(Type::Array)
{
	setArray(array);
}

Json::Value::Value(std::initializer_list<bool> array)
	: m_type(Type::Array)
{
	setArray(array);
}

Json::Value::Value(const Value& json)
	: m_type(json.m_type)
{
	switch (m_type)
	{
		case Type::String:
			new(&m_value.string) StringType(json.m_value.string);
			break;
		case Type::Float:
			m_value.floatNumber = json.m_value.floatNumber;
			break;
		case Type::Int:
			m_value.intNumber = json.m_value.intNumber;
			break;
		case Type::Boolean:
			m_value.boolean = json.m_value.boolean;
			break;
		case Type::Null:
			m_value.null = json.m_value.null;
			break;
		case Type::Object:
			new(&m_value.object) MapType(json.m_value.object);
			break;
		case Type::Array:
			new(&m_value.array) ArrayType(json.m_value.array);
			break;
		default:
			ASSERT_NOT_REACHED();
	}
}

Json::Value::Value(Value&& json) noexcept
	: m_type(json.m_type)
{
	switch (m_type)
	{
		case Type::String:
			new(&m_value.string) StringType(std::move(json.m_value.string));
			break;
		case Type::Float:
			m_value.floatNumber = json.m_value.floatNumber;
			break;
		case Type::Int:
			m_value.intNumber = json.m_value.intNumber;
			break;
		case Type::Boolean:
			m_value.boolean = json.m_value.boolean;
			break;
		case Type::Null:
			break;
		case Type::Object:
			new(&m_value.object) MapType(std::move(json.m_value.object));
			break;
		case Type::Array:
			new(&m_value.array) ArrayType(std::move(json.m_value.array));
			break;
		default:
			ASSERT_NOT_REACHED();
	}
}

Json::Value::~Value()
{
	destructValue();
}

Json::Value& Json::Value::operator=(const StringType& string)
{
	destructValue();
	m_type = Type::String;
	new(&m_value.string) StringType(string);
	return *this;
}

Json::Value& Json::Value::operator=(StringType&& string)
{
	destructValue();
	m_type = Type::String;
	new(&m_value.string) StringType(std::move(string));
	return *this;
}

Json::Value& Json::Value::operator=(const char* string)
{
	destructValue();
	m_type = Type::String;
	new(&m_value.string) StringType(string);
	return *this;
}

Json::Value& Json::Value::operator=(FloatType number)
{
	destructValue();
	m_type = Type::Float;
	m_value.floatNumber = number;
	return *this;
}

Json::Value& Json::Value::operator=(IntType number)
{
	destructValue();
	m_type = Type::Int;
	m_value.intNumber = number;
	return *this;
}

Json::Value& Json::Value::operator=(bool boolean)
{
	destructValue();
	m_type = Type::Boolean;
	m_value.boolean = boolean;
	return *this;
}

Json::Value& Json::Value::operator=(std::nullptr_t)
{
	destructValue();
	m_type = Type::Null;
	return *this;
}

Json::Value& Json::Value::operator=(const Value& object)
{
	if (this == &object)
		return *this;

	destructValue();

	m_type = object.m_type;

	switch (m_type)
	{
		case Type::String:
			new(&m_value.string) StringType(object.m_value.string);
			break;
		case Type::Float:
			m_value.floatNumber = object.m_value.floatNumber;
			break;
		case Type::Int:
			m_value.intNumber = object.m_value.intNumber;
			break;
		case Type::Boolean:
			m_value.boolean = object.m_value.boolean;
			break;
		case Type::Null:
			break;
		case Type::Object:
			new(&m_value.object) MapType(object.m_value.object);
			break;
		case Type::Array:
			new(&m_value.array) ArrayType(object.m_value.array);
			break;
		default:
			ASSERT_NOT_REACHED();
	}

	return *this;
}

Json::Value& Json::Value::operator=(Value&& object) noexcept
{
	if (this == &object)
		return *this;

	destructValue();
	m_type = object.m_type;

	switch (m_type)
	{
		case Type::String:
			new(&m_value.string) StringType(std::move(object.m_value.string));
			break;
		case Type::Float:
			m_value.floatNumber = object.m_value.floatNumber;
			break;
		case Type::Int:
			m_value.intNumber = object.m_value.intNumber;
			break;
		case Type::Boolean:
			m_value.boolean = object.m_value.boolean;
			break;
		case Type::Null:
			break;
		case Type::Object:
			new(&m_value.object) MapType(std::move(object.m_value.object));
			break;
		case Type::Array:
			new(&m_value.array) ArrayType(std::move(object.m_value.array));
			break;
		default:
			ASSERT_NOT_REACHED();
	}

	object.m_type = Type::Null;
	object.m_value.null = nullptr;

	return *this;
}

Json::Value& Json::Value::operator[](const StringType& key)
{
	return m_value.object[key];
}

Json::Value& Json::Value::operator[](StringType&& key) noexcept
{
	return m_value.object[std::move(key)];
}

const Json::Value& Json::Value::operator[] (const StringType& key) const
{
	return m_value.object.at(key);
}

Json::Value& Json::Value::at(const StringType& key)
{
	if (m_type != Type::Object)
		throw InvalidTypeAccess();

	if (contains(key) == false)
		throw OutOfRangeAccess();

	return m_value.object.at(key);
}

const Json::Value& Json::Value::at(const StringType& key) const
{
	if (m_type != Type::Object)
		throw InvalidTypeAccess();

	if (contains(key) == false)
		throw OutOfRangeAccess();

	return m_value.object.at(key);
}

bool Json::Value::contains(const StringType& key) const
{
	return m_value.object.count(key) > 0;
}

Json::Value::StringType& Json::Value::string()
{
	ASSERT(isString());
	return m_value.string;
}

Json::Value::FloatType& Json::Value::number()
{
	ASSERT(isFloat());
	return m_value.floatNumber;
}

bool& Json::Value::boolean()
{
	ASSERT(isBoolean());
	return m_value.boolean;
}

Json::Value::FloatType& Json::Value::floatNumber()
{
	ASSERT(isFloat());
	return m_value.floatNumber;
}

Json::Value::IntType& Json::Value::intNumber()
{
	ASSERT(isInt());
	return m_value.intNumber;
}

Json::Value::MapType& Json::Value::object()
{
	ASSERT(isObject());
	return m_value.object;
}

Json::Value::ArrayType& Json::Value::array()
{
	ASSERT(isArray());
	return m_value.array;
}

const Json::Value::StringType& Json::Value::string() const
{
	ASSERT(isString());
	return m_value.string;
}

Json::Value::FloatType Json::Value::number() const
{
	ASSERT(isNumber());
	if (isInt())
		return static_cast<double>(m_value.intNumber);
	return m_value.floatNumber;
}

Json::Value::FloatType Json::Value::floatNumber() const
{
	ASSERT(isFloat());
	return m_value.floatNumber;
}

Json::Value::IntType Json::Value::intNumber() const
{
	ASSERT(isInt());
	return m_value.intNumber;
}

bool Json::Value::boolean() const
{
	ASSERT(isBoolean());
	return m_value.boolean;
}

const Json::Value::MapType& Json::Value::object() const
{
	ASSERT(isObject());
	return m_value.object;
}

const Json::Value::ArrayType& Json::Value::array() const
{
	ASSERT(isArray());
	return m_value.array;
}

Json::Value::StringType& Json::Value::getString()
{
	if (isString())
		return m_value.string;
	throw InvalidTypeAccess();
}

double& Json::Value::getNumber()
{
	if (isFloat())
		return m_value.floatNumber;
	throw InvalidTypeAccess();
}

Json::Value::FloatType& Json::Value::getFloatNumber()
{
	if (isFloat())
		return m_value.floatNumber;
	throw InvalidTypeAccess();
}

Json::Value::IntType& Json::Value::getIntNumber()
{
	if (isInt())
		return m_value.intNumber;
	throw InvalidTypeAccess();
}

bool& Json::Value::getBoolean()
{
	if (isBoolean())
		return m_value.boolean;
	throw InvalidTypeAccess();
}

Json::Value::MapType& Json::Value::getObject()
{
	if (isObject())
		return m_value.object;
	throw InvalidTypeAccess();
}

Json::Value::ArrayType& Json::Value::getArray()
{
	if (isArray())
		return m_value.array;
	throw InvalidTypeAccess();
}

const Json::Value::StringType& Json::Value::getString() const
{
	if (isString())
		return m_value.string;
	throw InvalidTypeAccess();
}

Json::Value::FloatType Json::Value::getNumber() const
{
	if (isInt())
		return static_cast<FloatType>(m_value.intNumber);
	else if (isFloat())
		return m_value.floatNumber;
	throw InvalidTypeAccess();
}

Json::Value::FloatType Json::Value::getFloatNumber() const
{
	if (isFloat())
		return m_value.floatNumber;
	throw InvalidTypeAccess();
}

Json::Value::IntType Json::Value::getIntNumber() const
{
	if (isInt())
		return m_value.intNumber;
	throw InvalidTypeAccess();
}

bool Json::Value::getBoolean() const
{
	if (isBoolean())
		return m_value.boolean;
	throw InvalidTypeAccess();
}

const Json::Value::MapType& Json::Value::getObject() const
{
	if (isObject())
		return m_value.object;
	throw InvalidTypeAccess();
}

const Json::Value::ArrayType& Json::Value::getArray() const
{
	if (isArray())
		return m_value.array;
	throw InvalidTypeAccess();
}

void Json::Value::destructValue()
{
	switch (m_type)
	{
	case Type::String:
		m_value.string.~basic_string<char>();
		break;
	case Type::Object:
		m_value.object.~MapType();
		break;
	case Type::Array:
		m_value.array.~vector();
		break;
	}
}

Json::Value::Type Json::Value::type() const
{
	return m_type;
}

// Union destructors and constructors are implicitly deleted
Json::Value::Union::Union()
// This is useless I just don't want the warnings about uninitialized member variables
// and it would be more trouble to make a cross platform warning disable with #pragma.
	: null(nullptr)
{}

Json::Value::Union::Union(FloatType floatNumber)
	: floatNumber(floatNumber)
{}

Json::Value::Union::Union(IntType intNumber)
	: intNumber(intNumber)
{}

Json::Value::Union::Union(const std::string & string)
	: string(string)
{}

Json::Value::Union::Union(StringType&& string) noexcept
	: string(std::move(string))
{}

Json::Value::Union::Union(bool boolean)
	: boolean(boolean)
{}

Json::Value::Union::Union(std::nullptr_t null)
	: null(nullptr)
{}

Json::Value::Union::Union(MapType&& object) noexcept
	: object(std::move(object))
{}

Json::Value::Union::Union(ArrayType&& array) noexcept
	: array(std::move(array))
{}

Json::Value::Union::~Union()
{}

bool Json::Value::isString() const
{
	return m_type == Type::String;
}

bool Json::Value::isNumber() const
{
	return (m_type == Type::Float) || (m_type == Type::Int);
}

bool Json::Value::isInt() const
{
	return m_type == Type::Int;
}

bool Json::Value::isFloat() const
{
	return m_type == Type::Float;
}

bool Json::Value::isBoolean() const
{
	return m_type == Type::Boolean;
}

bool Json::Value::isNull() const
{
	return m_type == Type::Null;
}

bool Json::Value::isObject() const
{
	return m_type == Type::Object;
}

bool Json::Value::isArray() const
{
	return m_type == Type::Array;
}
