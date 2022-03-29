#pragma once

namespace Lang
{
	enum class ObjType
	{
		String
	};

	struct Obj
	{
		ObjType type;
	};

	struct ObjString
	{
		Obj obj;
		char* chars;
		size_t length;
	};
}