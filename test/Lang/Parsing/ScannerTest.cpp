#include <gtest/gtest.h>

#include <Lang/Parsing/Scanner.hpp>

using namespace Lang;

class ScannerTest : public ::testing::Test
{
protected:
	Scanner scanner;
};

TEST_F(ScannerTest, IntNumbers)
{
	auto tokens = scanner.parse("1234");
	ASSERT_EQ(tokens.size(), 2);
	EXPECT_EQ(tokens[0].type, TokenType::IntNumber);
	EXPECT_EQ(tokens[1].type, TokenType::Eof);
}

