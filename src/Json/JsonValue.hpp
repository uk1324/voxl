#pragma once

#include <unordered_map>

// Constructors and operators for const char* becuase without them it gets implicitly converted to bool.

namespace Json
{
	class Value
	{
	public:
		class InvalidTypeAccess : public std::exception {};
		class OutOfRangeAccess : public std::exception {};

		using StringType = std::string;
		using ArrayType = std::vector<Value>;
		using IntType = int;
		using FloatType = double;
		// Some implementations of std::map can cause errors because of recursive types.
		// TODO: Fix this.
		using MapType = std::unordered_map<StringType, Value>;

		Value();
		static Value emptyObject();
		static Value emptyArray();
		static Value null();

		Value(const StringType& string);
		Value(StringType&& string) noexcept;
		Value(const char* string);
		Value(FloatType number);
		Value(IntType number);
		Value(bool boolean);
		Value(std::nullptr_t);
		Value(std::initializer_list<std::pair<const StringType, Value>> object);

		Value(std::initializer_list<FloatType> array);
		Value(std::initializer_list<IntType> array);
		Value(std::initializer_list<bool> array);

		Value(const Value& json);
		Value(Value&& json) noexcept;

		~Value();

		Value& operator= (const StringType& string);
		Value& operator= (StringType&& string);
		Value& operator= (const char* string);
		Value& operator= (FloatType number);
		Value& operator= (IntType number);
		Value& operator= (bool boolean);
		Value& operator= (std::nullptr_t);
		Value& operator= (const Value& object);
		Value& operator= (Value&& object) noexcept;

		// Exibits undefined behaviour if this->m_type is not an object.
		// If key does not exists creates it.
		Value& operator[] (const StringType& key);
		Value& operator[] (StringType&& key) noexcept;
		const Value& operator[] (const StringType& key) const;

		// Throws InvalidTypeAccess when not this->type != Type::Object.
		// Throws OutOfRangeAccess when key doesn't exist.
		Value& at(const StringType& key);
		const Value& at(const StringType& key) const;

		bool contains(const StringType& key) const;

		bool isNumber() const;
		bool isFloat() const;
		bool isInt() const;
		bool isString() const;
		bool isBoolean() const;
		bool isNull() const;
		bool isObject() const;
		bool isArray() const;

		// Exibits undefined behaviour if the type is not correct
		StringType& string();
		FloatType& number();
		FloatType& floatNumber();
		IntType& intNumber();
		bool& boolean();
		MapType& object();
		std::vector<Value>& array();

		const StringType& string() const;
		FloatType number() const;
		FloatType floatNumber() const;
		IntType intNumber() const;
		bool boolean() const;
		const MapType& object() const;
		const ArrayType& array() const;

		// Type checked access
		StringType& getString();
		FloatType& getNumber();
		FloatType& getFloatNumber();
		IntType& getIntNumber();
		bool& getBoolean();
		MapType& getObject();
		ArrayType& getArray();

		const StringType& getString() const;
		FloatType getNumber() const;
		FloatType getFloatNumber() const;
		IntType getIntNumber() const;
		bool getBoolean() const;
		const MapType& getObject() const;
		const ArrayType& getArray() const;

	private:
		template <typename T>
		void setArray(std::initializer_list<T> array);

		void destructValue();

		union Union
		{
			Union();
			Union(FloatType floatNumber);
			Union(IntType intNumber);
			Union(const std::string& string);
			Union(StringType&& string) noexcept;
			Union(bool boolean);
			Union(std::nullptr_t null);
			Union(MapType&& object) noexcept;
			Union(ArrayType&& array) noexcept;
			~Union();

			FloatType floatNumber;
			IntType intNumber;
			StringType string;
			bool boolean;
			std::nullptr_t null;
			MapType object;
			ArrayType array;
		} m_value;

	public:
		enum class Type
		{
			String,
			Float,
			Int,
			Boolean,
			Null,
			Object,
			Array,
		};

		Type type() const;

	private:
		Type m_type;
	};
}

template<typename T>
void Json::Value::setArray(std::initializer_list<T> array)
{
	new(&m_value.array) ArrayType();
	m_value.array.reserve(array.size());
	for (T item : array)
		m_value.array.push_back(Value(item));
}