#include <Vm/Dict.hpp>
#include <Context.hpp>

using namespace Voxl;

//LocalValue Dict::get_index(Context& c)
//{
//	const auto self = c.args(0).asObj<Dict>();
//	const auto value = c.args(1);
//	self->dict
//}
//
//LocalValue Dict::set_index(Context& c)
//{
//
//}
//
//void Dict::init(Dict* self)
//{
//	new (&self->dict) std::unordered_map<Key, Value>();
//}
//
//void Dict::free(Dict* self)
//{
//	self->~Dict();
//}
//
//void Dict::mark(Dict* self, Allocator& allocator)
//{
//	for (auto& [key, value] : self->dict)
//	{
//		allocator.addValue(key.value);
//		allocator.addValue(value);
//	}
//}
