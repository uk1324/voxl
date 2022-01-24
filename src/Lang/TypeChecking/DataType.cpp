#include <Lang/TypeChecking/DataType.hpp>
#include <Utils/Assertions.hpp>

using namespace Lang;

DataType::DataType()
	: DataType(DataTypeType::Dynamic)
{}

DataType::DataType(DataTypeType type)
	: type(type)
{}

const char* Lang::dataTypeTypeToString(DataTypeType type)
{
	switch (type)
	{
	case DataTypeType::Dynamic: return "Dynamic";
	case DataTypeType::Int: return "Int";
	case DataTypeType::Float: return "Float";
	default:
		ASSERT_NOT_REACHED();
		return "";
	}
}
