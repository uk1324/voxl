#include <KeyTraits.hpp>
#include <Value.hpp>

using namespace Lang;

bool ObjStringKeyTraits::compareKeys(const ObjString* a, const ObjString* b)
{
	return (a->size == b->size) && (memcmp(a->chars, b->chars, a->size) == 0);
}

size_t ObjStringKeyTraits::hashKey(const ObjString* key)
{
	return std::hash<std::string_view>()(std::string_view(key->chars, key->size));
}

void ObjStringKeyTraits::setKeyNull(ObjString*& key)
{
	key = nullptr;
}

void ObjStringKeyTraits::setKeyTombstone(ObjString*& key)
{
	key = reinterpret_cast<ObjString*>(1);
}

bool ObjStringKeyTraits::isKeyNull(const ObjString* key)
{
	return key == nullptr;
}

bool ObjStringKeyTraits::isKeyTombstone(const ObjString* key)
{
	return key == reinterpret_cast<ObjString*>(1);
}