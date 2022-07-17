#include <Value.hpp>
#include <Obj.hpp>
#include <Context.hpp>
#include <Asserts.hpp>

using namespace Lang;

Value::Value(Int value)
	: type(ValueType::Int)
{
	as.intNumber = value;
}

Value::Value(Float value)
	: type(ValueType::Float)
{
	as.floatNumber = value;
}

Value::Value(Obj* obj)
	: type(ValueType::Obj)
{
	as.obj = obj;
}

Value::Value(bool boolean)
	: type(ValueType::Bool)
{
	as.boolean = boolean;
}

Value Value::null()
{
	Value value;
	value.type = ValueType::Null;
	return value;
}

Value Value::integer(Int value)
{
	return Value(value);
}

std::ostream& operator<<(std::ostream& os, Value value)
{
	using namespace Lang;

	switch (value.type)
	{
		case ValueType::Int:
			os << value.as.intNumber;
			break;

		case ValueType::Float:
			os << value.as.floatNumber;
			break;

		case ValueType::Null:
			os << "null";
			break;

		case ValueType::Bool:
			os << (value.as.boolean ? "true" : "false");
			break;

		case ValueType::Obj:
			os << value.as.obj;
			break;

	default:
		ASSERT_NOT_REACHED();
	}

	return os;
}

std::ostream& operator<< (std::ostream& os, Lang::Obj* obj)
{
	switch (obj->type)
	{
		case ObjType::String:
		{
			os << obj->asString()->chars;
			break;
		}

		case ObjType::Function:
		{
			const auto function = obj->asFunction();
			os << '<' << function->name->chars << '>';
			break;
		}

		case ObjType::NativeFunction:
		{
			const auto function = obj->asNativeFunction();
			os << '<' << function->name->chars << '>';
			break;
		}

		case ObjType::Class:
		{
			const auto class_ = obj->asClass();
			os << "<class '" << class_->name->chars << "'>";
			break;
		}

		case ObjType::Instance:
		{
			const auto instance = obj->asInstance();
			os << "<instance of '" << instance->class_->name->chars << "'>";
			break;
		}

		case ObjType::NativeInstance:
		{
			const auto instance = obj->asNativeInstance();
			os << "<native instance of '" << instance->class_->name->chars << "'>";
			break;
		}

		case ObjType::BoundFunction:
			os << obj->asBoundFunction()->callable;
			break;

		case ObjType::Closure:
		{
			auto closure = obj->asClosure();
			os << '<' << "closure of " << closure->function->name->chars << '>';
			break;
		}

		case ObjType::Module:
		{
			os << "<module>";
			break;
		}

		default:
			ASSERT_NOT_REACHED();
	}

	return os;
}

bool operator==(const Value& lhs, const Value& rhs)
{
	if (lhs.type != rhs.type)
		return false;

	switch (lhs.type)
	{
		case ValueType::Int: return lhs.as.intNumber == rhs.as.intNumber;
		case ValueType::Float: return lhs.as.floatNumber == rhs.as.floatNumber;
		//case ValueType::Obj: return lhs.as.floatNumber == rhs.as.ob;
		case ValueType::Null: return true;
		case ValueType::Bool: return lhs.as.boolean == rhs.as.boolean;

		default:
			ASSERT_NOT_REACHED();
			return false;
	}
}

NativeException::NativeException(const LocalValue& value)
	: value(value.value)
{}

NativeException::NativeException(const Value& value)
	: value(value)
{}
