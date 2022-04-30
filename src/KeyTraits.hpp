#pragma once

namespace Lang
{

struct ObjString;

struct ObjStringKeyTraits
{
	static bool compareKeys(const ObjString* a, const ObjString* b);
	static size_t hashKey(const ObjString* key);
	static void setKeyNull(ObjString*& key);
	static void setKeyTombstone(ObjString*& key);
	static bool isKeyNull(const ObjString* key);
	static bool isKeyTombstone(const ObjString* key);
};

}