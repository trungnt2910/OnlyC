#include <cstdlib>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "core/LyricLink.h"
}

class Given_LyricLink : public ::testing::Test
{
protected:
    Song* song = nullptr;

    void SetUp() override
    {
        song = (Song*)calloc(1, sizeof(*song));
        ASSERT_NE(song, nullptr);
        song->count = 3;
        strcpy(song->sections[0].translated, "One\nTwo\n\nFour\n");
        strcpy(song->sections[1].translated, "Five\nSix\nSeven");
        strcpy(song->sections[2].translated, "Eight\nNine");
        song->range_count = 2;
        song->ranges[0] = {0, 2, 3, "shared"};
        song->ranges[1] = {1, 1, 2, "shared"};
    }

    void TearDown() override
    {
        free(song);
    }
};

typedef struct
{
    const char* fragment;
    int section;
    size_t line;
} LinkCase;

class Given_LyricLinkResolution : public Given_LyricLink,
                                  public ::testing::WithParamInterface<LinkCase>
{
};

TEST_P(Given_LyricLinkResolution, When_FragmentOverlapsLyrics_Then_FirstMatchingItemIsResolved)
{
    LinkCase input = GetParam();
    LyricTarget target = {-1, 0};

    int found = lyric_link_resolve(song, input.fragment, &target);

    EXPECT_TRUE(found);
    EXPECT_EQ(target.section, input.section);
    EXPECT_EQ(target.line, input.line);
}

INSTANTIATE_TEST_SUITE_P(LineAndTitleFragments,
                         Given_LyricLinkResolution,
                         ::testing::Values(LinkCase{"#L1", 0, 1},
                                           LinkCase{"#L3", 0, 2},
                                           LinkCase{"#L3-L8", 0, 2},
                                           LinkCase{"#L4-L6", 0, 4},
                                           LinkCase{"#L5", 1, 1},
                                           LinkCase{"#L6", 1, 1},
                                           LinkCase{"#L7", 1, 3},
                                           LinkCase{"#L8", 2, 1},
                                           LinkCase{"#L9-L12", 2, 2},
                                           LinkCase{"#title", TITLE_SECTION, 1}));

class Given_LyricLinkValidation : public Given_LyricLink,
                                  public ::testing::WithParamInterface<const char*>
{
};

TEST_P(Given_LyricLinkValidation, When_FragmentIsInvalid_Then_OutputIsPreserved)
{
    LyricTarget target = {17, 42};

    int found = lyric_link_resolve(song, GetParam(), &target);

    EXPECT_FALSE(found);
    EXPECT_EQ(target.section, 17);
    EXPECT_EQ(target.line, 42u);
}

INSTANTIATE_TEST_SUITE_P(MalformedAndNonoverlappingFragments,
                         Given_LyricLinkValidation,
                         ::testing::Values("",
                                           "#",
                                           "#L",
                                           "#L0",
                                           "#L10",
                                           "#L10-L12",
                                           "#L8193",
                                           "#L999999999999999999999",
                                           "#L3-L2",
                                           "#L1-",
                                           "#L1-L",
                                           "#L1-2",
                                           "#L1-L0",
                                           "#L1-L8193",
                                           "#l1",
                                           "#L-1",
                                           "#L1 ",
                                           "#L1-L2-L3",
                                           "#L1%2DL2",
                                           "#Title"));

typedef struct
{
    int section;
    size_t line;
    const char* fragment;
} SelectionCase;

class Given_LyricLinkFormatting : public Given_LyricLink,
                                  public ::testing::WithParamInterface<SelectionCase>
{
};

TEST_P(Given_LyricLinkFormatting, When_SelectionIsFormatted_Then_FullItemRangeIsUsed)
{
    SelectionCase input = GetParam();
    char fragment[40];

    int formatted = lyric_link_format(song, input.section, input.line, fragment, sizeof(fragment));

    EXPECT_TRUE(formatted);
    EXPECT_STREQ(fragment, input.fragment);
}

INSTANTIATE_TEST_SUITE_P(SelectionFragments,
                         Given_LyricLinkFormatting,
                         ::testing::Values(SelectionCase{0, 1, "#L1"},
                                           SelectionCase{0, 3, "#L2-L3"},
                                           SelectionCase{0, 4, "#L4"},
                                           SelectionCase{1, 2, "#L5-L6"},
                                           SelectionCase{2, 1, "#L8"},
                                           SelectionCase{TITLE_SECTION, 1, "#title"},
                                           SelectionCase{-1, 0, ""}));

TEST_F(Given_LyricLink, When_BufferCannotHoldWholeFragment_Then_PartialLinkIsCleared)
{
    char line[4] = "old", title[4] = "old";

    int line_result = lyric_link_format(song, 0, 2, line, sizeof(line));
    int title_result = lyric_link_format(song, TITLE_SECTION, 1, title, sizeof(title));

    EXPECT_FALSE(line_result);
    EXPECT_FALSE(title_result);
    EXPECT_STREQ(line, "");
    EXPECT_STREQ(title, "");
}

TEST_F(Given_LyricLink, When_SelectionIsOutsideLyrics_Then_NoLinkIsEmitted)
{
    char section[40] = "old", zero[40] = "old", line[40] = "old";

    int section_result = lyric_link_format(song, 3, 1, section, sizeof(section));
    int zero_result = lyric_link_format(song, 0, 0, zero, sizeof(zero));
    int line_result = lyric_link_format(song, 0, 5, line, sizeof(line));

    EXPECT_FALSE(section_result);
    EXPECT_FALSE(zero_result);
    EXPECT_FALSE(line_result);
    EXPECT_STREQ(section, "");
    EXPECT_STREQ(zero, "");
    EXPECT_STREQ(line, "");
}

TEST_F(Given_LyricLink, When_LastPossibleLineIsLinked_Then_MaximumNumberIsAccepted)
{
    song->count = MAX_SECTIONS;
    for (size_t i = 0; i < MAX_SECTIONS; ++i)
    {
        memset(song->sections[i].translated, '\n', MAX_LINES - 1);
        song->sections[i].translated[MAX_LINES - 1] = 'x';
        song->sections[i].translated[MAX_LINES] = 0;
    }
    LyricTarget target = {-1, 0};
    char fragment[40];

    int found = lyric_link_resolve(song, "#L8192", &target);
    int formatted =
        lyric_link_format(song, MAX_SECTIONS - 1, MAX_LINES, fragment, sizeof(fragment));

    EXPECT_TRUE(found);
    EXPECT_EQ(target.section, MAX_SECTIONS - 1);
    EXPECT_EQ(target.line, (size_t)MAX_LINES);
    EXPECT_TRUE(formatted);
    EXPECT_STREQ(fragment, "#L8192");
}
