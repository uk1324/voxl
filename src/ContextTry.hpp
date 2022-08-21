#define TRY(somethingThatReturnsResult) \
	do \
	{ \
		const auto expressionResult = somethingThatReturnsResult; \
		switch (expressionResult.type) \
		{ \
		case Vm::ResultType::Exception: \
			throw NativeException(expressionResult.exceptionValue); \
		case Vm::ResultType::Fatal: \
			throw Vm::FatalException(); \
		case Vm::ResultType::ExceptionHandled: \
		case Vm::ResultType::Ok: \
			break; \
		} \
	} while(false)

#define TRY_WITH_VALUE(resultWithValue) \
	do \
	{ \
		const auto expressionResult = resultWithValue; \
		switch (resultWithValue.type) \
		{ \
		case Vm::ResultType::Exception: \
			throw NativeException(expressionResult.value); \
		case Vm::ResultType::Fatal: \
			throw Vm::FatalException(); \
		case Vm::ResultType::ExceptionHandled: \
		case Vm::ResultType::Ok: \
			break; \
		} \
	} while(false)