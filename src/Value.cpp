#include <Value.hpp>
#include <Asserts.hpp>

using namespace Lang;

Value::Value(Int value)
	: type(ValueType::Int)
{
	as.intNumber = value;
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
			switch (value.as.obj->type)
			{
				case ObjType::String:
				{
					const auto string = reinterpret_cast<ObjString*>(value.as.obj);
					for (size_t i = 0; i < string->size; i++)
					{
						os << string->chars[i];
					}
					break;
				}

				case ObjType::Function:
				{
					const auto function = reinterpret_cast<ObjFunction*>(value.as.obj);
					os << '<';
					for (size_t i = 0; i < function->name->size; i++)
					{
						os << function->name->chars[i];
					}
					os << '>';
					break;
				}

				case ObjType::ForeignFunction:
				{
					const auto function = reinterpret_cast<ObjForeignFunction*>(value.as.obj);
					os << '<';
					for (size_t i = 0; i < function->name->size; i++)
					{
						os << function->name->chars[i];
					}
					os << '>';
					break;
				}

				default:
					ASSERT_NOT_REACHED();
			}
			break;

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
