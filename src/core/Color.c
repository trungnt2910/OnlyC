#include "core/Color.h"

#include <stdio.h>
#include <string.h>

static int digit(char value)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f')
    {
        return value - 'a' + 10;
    }
    return value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1;
}

bool color_parse(const char* text, Color* color)
{
    if (!text || !color || strlen(text) != 7 || text[0] != '#')
    {
        return false;
    }
    unsigned char channels[3];
    for (size_t i = 0; i < 3; ++i)
    {
        int high = digit(text[2 * i + 1]), low = digit(text[2 * i + 2]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        channels[i] = (unsigned char)(16 * high + low);
    }
    *color = (Color){channels[0], channels[1], channels[2]};
    return true;
}

void color_format(Color color, char text[8])
{
    snprintf(
        text, 8, "#%02x%02x%02x", (unsigned)color.red, (unsigned)color.green, (unsigned)color.blue);
}

bool color_similar(const char* first, const char* second)
{
    Color a, b;
    if (!color_parse(first, &a) || !color_parse(second, &b))
    {
        return false;
    }
    int red = (int)a.red - b.red, green = (int)a.green - b.green, blue = (int)a.blue - b.blue;
    /* Canvas decoding/color management can differ slightly across browsers.
     * Keep the authored seed for these small differences to avoid a color flash. */
    return red * red + green * green + blue * blue <= 12 * 12;
}

void color_mix(Color first, Color second, unsigned percent, char output[8])
{
    if (percent > 100)
    {
        percent = 100;
    }
    Color mixed = {
        .red = (unsigned char)((first.red * percent + second.red * (100 - percent) + 50) / 100),
        .green =
            (unsigned char)((first.green * percent + second.green * (100 - percent) + 50) / 100),
        .blue = (unsigned char)((first.blue * percent + second.blue * (100 - percent) + 50) / 100)};
    color_format(mixed, output);
}
