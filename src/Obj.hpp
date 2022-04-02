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
	const char* chars;
	size_t length;
};

}