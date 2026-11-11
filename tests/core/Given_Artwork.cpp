#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "core/Artwork.h"
#include "core/Color.h"
}

class Given_Artwork : public testing::Test
{
};

class Given_Color : public testing::Test
{
};

TEST_F(Given_Artwork, When_OneOpaqueColor_Then_ItIsDetected)
{
    unsigned char pixels[] = {80, 160, 200, 255, 80, 160, 200, 255};
    char color[8] = {};

    bool detected = artwork_color(pixels, 2, color);

    EXPECT_TRUE(detected);
    EXPECT_TRUE(color_similar(color, "#50a0c8"));
}

TEST_F(Given_Artwork, When_TransparencyAndBlackDominate_Then_VisibleColorIsSelected)
{
    unsigned char pixels[] = {0,   0, 0, 255, 0,   0, 0, 255, 0,  0,   0,   255,
                              255, 0, 0, 0,   255, 0, 0, 0,   80, 160, 200, 255};
    char color[8] = {};

    bool detected = artwork_color(pixels, 6, color);

    EXPECT_TRUE(detected);
    EXPECT_TRUE(color_similar(color, "#50a0c8"));
}

TEST_F(Given_Artwork, When_OnlyWhitePixels_Then_NeutralFallbackSucceeds)
{
    unsigned char pixels[] = {255, 255, 255, 255};
    char color[8] = {};

    bool detected = artwork_color(pixels, 1, color);

    EXPECT_TRUE(detected);
    EXPECT_TRUE(color_similar(color, "#ffffff"));
}

TEST_F(Given_Artwork, When_EmptyTransparentOrOversizedInput_Then_NoColorIsWritten)
{
    unsigned char pixels[] = {80, 160, 200, 0};
    char color[8] = "#123456";

    bool empty = artwork_color(pixels, 0, color);
    bool absent = artwork_color(nullptr, 1, color);
    bool transparent = artwork_color(pixels, 1, color);
    bool oversized = artwork_color(pixels, ARTWORK_SAMPLE_PIXELS + 1, color);
    bool no_output = artwork_color(pixels, 1, nullptr);

    EXPECT_FALSE(empty);
    EXPECT_FALSE(absent);
    EXPECT_FALSE(transparent);
    EXPECT_FALSE(oversized);
    EXPECT_FALSE(no_output);
    EXPECT_STREQ(color, "#123456");
}

TEST_F(Given_Color, When_MixedCaseHex_Then_ChannelsAndCanonicalFormArePreserved)
{
    Color color = {};
    char text[8] = {};

    bool parsed = color_parse("#aB12Ef", &color);
    color_format(color, text);

    EXPECT_TRUE(parsed);
    EXPECT_EQ(color.red, 171);
    EXPECT_EQ(color.green, 18);
    EXPECT_EQ(color.blue, 239);
    EXPECT_STREQ(text, "#ab12ef");
}

TEST_F(Given_Color, When_InvalidHex_Then_ParseFails)
{
    Color color = {};

    bool short_hex = color_parse("#123", &color);
    bool bad_digit = color_parse("#12345g", &color);
    bool no_hash = color_parse("1234567", &color);
    bool absent = color_parse(nullptr, &color);
    bool no_output = color_parse("#123456", nullptr);

    EXPECT_FALSE(short_hex);
    EXPECT_FALSE(bad_digit);
    EXPECT_FALSE(no_hash);
    EXPECT_FALSE(absent);
    EXPECT_FALSE(no_output);
}

TEST_F(Given_Color, When_SamplingDiffersSlightly_Then_SeedIsRetainedWithinTolerance)
{
    bool near = color_similar("#506070", "#566676");
    bool boundary = color_similar("#506070", "#5c6070");
    bool far = color_similar("#506070", "#5d6070");
    bool missing = color_similar("#506070", "");

    EXPECT_TRUE(near);
    EXPECT_TRUE(boundary);
    EXPECT_FALSE(far);
    EXPECT_FALSE(missing);
}

TEST_F(Given_Artwork, When_SeveralDistinctColors_Then_MostPopulatedSwatchWins)
{
    unsigned char pixels[] = {200, 40,  40, 255, 200, 40,  40, 255, 200, 40, 40,
                              255, 200, 40, 40,  255, 200, 40, 40,  255, 40, 200,
                              40,  255, 40, 200, 40,  255, 40, 40,  200, 255};
    char color[8] = {};

    bool detected = artwork_color(pixels, 8, color);

    EXPECT_TRUE(detected);
    EXPECT_STREQ(color, "#c82828");
}

TEST_F(Given_Artwork, When_DenseRgbSample_Then_DetectionIsDeterministicAndBounded)
{
    unsigned char pixels[ARTWORK_SAMPLE_BYTES];
    for (unsigned i = 0; i < ARTWORK_SAMPLE_PIXELS; ++i)
    {
        pixels[4 * i] = (unsigned char)(i * 13);
        pixels[4 * i + 1] = (unsigned char)(i * 29);
        pixels[4 * i + 2] = (unsigned char)(i * 47);
        pixels[4 * i + 3] = 255;
    }
    char first[8] = {}, second[8] = {};
    Color parsed = {};

    bool detected = artwork_color(pixels, ARTWORK_SAMPLE_PIXELS, first);
    bool repeated = artwork_color(pixels, ARTWORK_SAMPLE_PIXELS, second);
    bool valid = color_parse(first, &parsed);

    EXPECT_TRUE(detected);
    EXPECT_TRUE(repeated);
    EXPECT_TRUE(valid);
    EXPECT_STREQ(first, second);
}
