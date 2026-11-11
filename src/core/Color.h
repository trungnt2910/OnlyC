#ifndef ONLYC_CORE_COLOR_H
#define ONLYC_CORE_COLOR_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    unsigned char red, green, blue;
} Color;

bool color_parse(const char* text, Color* color);
void color_format(Color color, char text[8]);
bool color_similar(const char* first, const char* second);
void color_mix(Color first, Color second, unsigned percent, char output[8]);

#endif
