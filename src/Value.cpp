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

Value Value::null()
{
	Value value;
	value.type = ValueType::Null;
	return value;
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

		case ValueType::Obj:
			switch (value.as.obj->type)
			{
				case ObjType::String:
				{
					const auto string = reinterpret_cast<ObjString*>(value.as.obj);
					os << std::string_view(string->chars, string->length);
					break;
				}

				case ObjType::Function:
				{
					const auto function = reinterpret_cast<ObjFunction*>(value.as.obj);
					os << '<' << std::string_view(function->name->chars, function->name->length) << '>';
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
