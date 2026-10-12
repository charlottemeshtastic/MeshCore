#include <gtest/gtest.h>
#include <helpers/TxtDataHelpers.h>

TEST(TextFormatting, IntegerAndFractionalPartsKeepDecimalRepresentation) {
  EXPECT_STREQ("0.0", StrHelper::ftoa(0.0f));
  EXPECT_STREQ("123.0", StrHelper::ftoa(123.0f));
  EXPECT_STREQ("-123.0", StrHelper::ftoa(-123.0f));
  EXPECT_STREQ("12.5", StrHelper::ftoa(12.5f));
  EXPECT_STREQ("-12.5", StrHelper::ftoa(-12.5f));
  EXPECT_STREQ("2147483520.0", StrHelper::ftoa(2147483520.0f));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
