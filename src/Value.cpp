#include <Value.hpp>
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

bool Value::isInt() const
{
	return type == ValueType::Int;
}

bool Value::isFloat() const
{
	return type == ValueType::Float;
}

bool Value::isObj() const
{
	return type == ValueType::Obj;
}

bool Value::isNull() const
{
	return type == ValueType::Null;
}

bool Value::isBool() const
{
	return type == ValueType::Bool;
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
			const auto string = reinterpret_cast<ObjString*>(obj);
			for (size_t i = 0; i < string->size; i++)
			{
				os << string->chars[i];
			}
			break;
		}

		case ObjType::Function:
		{
			const auto function = reinterpret_cast<ObjFunction*>(obj);
			os << '<' << reinterpret_cast<Obj*>(function->name) << '>';
			break;
		}

		case ObjType::NativeFunction:
		{
			const auto function = reinterpret_cast<ObjNativeFunction*>(obj);
			os << '<' << reinterpret_cast<Obj*>(function->name) << '>';
			break;
		}

		case ObjType::Class:
		{
			auto class_ = obj->asClass();
			os << "<class '" << reinterpret_cast<Obj*>(class_->name) << "'>";
			break;
		}

		case ObjType::Instance:
		{
			auto instance = obj->asInstance();
			os << "<instance of '" << reinterpret_cast<Obj*>(instance->class_->name) << "'>";
			break;
		}

		case ObjType::NativeInstance:
		{
			auto instance = obj->asNativeInstance();
			os << "<native instance of '" << reinterpret_cast<Obj*>(instance->class_->name) << "'>";
			break;
		}

		case ObjType::BoundFunction:
			os << obj->asBoundFunction()->callable;
			break;

		case ObjType::Closure:
		{
			auto closure = obj->asClosure();
			os << '<' << "closure of " << reinterpret_cast<Obj*>(closure->function->name) << '>';
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