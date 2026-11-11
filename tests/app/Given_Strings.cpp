#include <gtest/gtest.h>

#include "StringsProbe.h"

class Given_Strings : public testing::Test
{
};

TEST_F(Given_Strings, When_CatalogContainsSpecialCharacters_Then_GeneratedLiteralPreservesTheText)
{
    const char expected[] =
        "Quotes: \"text\"; path: C:\\songs\\demo; @ONLYC_SITE_ORIGIN@; $<CONFIG>; \?\?/\n"
        "Tiếng Việt\t100%";

    const char* actual = ONLYC_STRING_TEST_ESCAPES;

    EXPECT_STREQ(actual, expected);
}

TEST_F(Given_Strings, When_ConfigOverridesDefault_Then_GeneratedLiteralUsesTheOverride)
{
    const char expected[] = "Override; \"quoted\" \\ value";

    const char* actual = ONLYC_STRING_TEST_OVERRIDE;

    EXPECT_STREQ(actual, expected);
}

TEST_F(Given_Strings, When_CatalogValueIsEmpty_Then_GeneratedLiteralIsAnEmptyString)
{
    const char expected[] = "";

    const char* actual = ONLYC_STRING_TEST_EMPTY;

    EXPECT_STREQ(actual, expected);
}
