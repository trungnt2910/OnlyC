#include "core/Artwork.h"

#include <stdlib.h>

#include "core/Color.h"

/* Median cut over a five-bit RGB histogram. Sampling and quantization have
 * fixed bounds; no image decoder or platform I/O is required here. */
enum
{
    CHANNEL_BITS = 5,
    CHANNEL_LEVELS = 1 << CHANNEL_BITS,
    HISTOGRAM_SIZE = 1 << (3 * CHANNEL_BITS),
    PALETTE_SIZE = 8
};

typedef struct
{
    unsigned count;
    unsigned channels[3];
} ColorBin;

typedef struct
{
    unsigned char low[3];
    unsigned char high[3];
    unsigned count;
} ColorBox;

static unsigned channel_at(unsigned index, unsigned channel)
{
    return (index >> ((2 - channel) * CHANNEL_BITS)) & (CHANNEL_LEVELS - 1);
}

static int contains(const ColorBox* box, unsigned index)
{
    for (unsigned channel = 0; channel < 3; ++channel)
    {
        unsigned value = channel_at(index, channel);
        if (value < box->low[channel] || value > box->high[channel])
        {
            return 0;
        }
    }
    return 1;
}

static void measure(ColorBox* box, const ColorBin* bins)
{
    unsigned char low[3] = {31, 31, 31}, high[3] = {0};
    box->count = 0;
    for (unsigned i = 0; i < HISTOGRAM_SIZE; ++i)
    {
        if (!bins[i].count || !contains(box, i))
        {
            continue;
        }
        box->count += bins[i].count;
        for (unsigned channel = 0; channel < 3; ++channel)
        {
            unsigned char value = (unsigned char)channel_at(i, channel);
            if (value < low[channel])
            {
                low[channel] = value;
            }
            if (value > high[channel])
            {
                high[channel] = value;
            }
        }
    }
    for (unsigned channel = 0; channel < 3; ++channel)
    {
        box->low[channel] = low[channel];
        box->high[channel] = high[channel];
    }
}

static unsigned widest_channel(const ColorBox* box)
{
    unsigned widest = 0;
    for (unsigned channel = 1; channel < 3; ++channel)
    {
        if (box->high[channel] - box->low[channel] > box->high[widest] - box->low[widest])
        {
            widest = channel;
        }
    }
    return widest;
}

static void split(ColorBox* first, ColorBox* second, const ColorBin* bins)
{
    unsigned axis = widest_channel(first);
    unsigned populations[CHANNEL_LEVELS] = {0};
    for (unsigned i = 0; i < HISTOGRAM_SIZE; ++i)
    {
        if (bins[i].count && contains(first, i))
        {
            populations[channel_at(i, axis)] += bins[i].count;
        }
    }
    unsigned cut = first->low[axis], sum = 0;
    for (; cut < first->high[axis]; ++cut)
    {
        sum += populations[cut];
        if (sum * 2 >= first->count)
        {
            break;
        }
    }
    /* Leave an occupied bin on both sides even when one color dominates. */
    if (cut == first->high[axis])
    {
        --cut;
    }
    *second = *first;
    first->high[axis] = (unsigned char)cut;
    second->low[axis] = (unsigned char)(cut + 1);
    measure(first, bins);
    measure(second, bins);
}

static void add_pixel(ColorBin* bins, const unsigned char* pixel)
{
    unsigned index =
        ((unsigned)(pixel[0] >> 3) << 10) | ((unsigned)(pixel[1] >> 3) << 5) | (pixel[2] >> 3);
    ++bins[index].count;
    for (unsigned channel = 0; channel < 3; ++channel)
    {
        bins[index].channels[channel] += pixel[channel];
    }
}

bool artwork_color(const unsigned char* rgba, size_t pixels, char color[8])
{
    if (!rgba || !color || !pixels || pixels > ARTWORK_SAMPLE_PIXELS)
    {
        return false;
    }
    ColorBin* bins = calloc(HISTOGRAM_SIZE, sizeof(*bins));
    if (!bins)
    {
        return false;
    }
    unsigned opaque = 0, useful = 0;
    for (size_t i = 0; i < pixels; ++i)
    {
        const unsigned char* pixel = rgba + 4 * i;
        int extreme = (pixel[0] < 24 && pixel[1] < 24 && pixel[2] < 24) ||
                      (pixel[0] > 240 && pixel[1] > 240 && pixel[2] > 240);
        if (pixel[3] >= 128)
        {
            ++opaque;
            if (!extreme)
            {
                add_pixel(bins, pixel);
                ++useful;
            }
        }
    }
    if (!opaque)
    {
        free(bins);
        return false;
    }
    if (!useful)
    {
        for (size_t i = 0; i < pixels; ++i)
        {
            if (rgba[4 * i + 3] >= 128)
            {
                add_pixel(bins, rgba + 4 * i);
            }
        }
    }
    ColorBox boxes[PALETTE_SIZE] = {{{0, 0, 0}, {31, 31, 31}, 0}};
    measure(&boxes[0], bins);
    unsigned count = 1;
    while (count < PALETTE_SIZE)
    {
        unsigned best = 0, score = 0;
        for (unsigned i = 0; i < count; ++i)
        {
            unsigned axis = widest_channel(&boxes[i]);
            unsigned range = boxes[i].high[axis] - boxes[i].low[axis];
            unsigned candidate = boxes[i].count * range;
            if (candidate > score)
            {
                score = candidate;
                best = i;
            }
        }
        if (!score)
        {
            break;
        }
        split(&boxes[best], &boxes[count++], bins);
    }
    unsigned best = 0;
    for (unsigned i = 1; i < count; ++i)
    {
        if (boxes[i].count > boxes[best].count)
        {
            best = i;
        }
    }
    unsigned sums[3] = {0};
    for (unsigned i = 0; i < HISTOGRAM_SIZE; ++i)
    {
        if (bins[i].count && contains(&boxes[best], i))
        {
            for (unsigned channel = 0; channel < 3; ++channel)
            {
                sums[channel] += bins[i].channels[channel];
            }
        }
    }
    unsigned population = boxes[best].count;
    Color result = {(unsigned char)((sums[0] + population / 2) / population),
                    (unsigned char)((sums[1] + population / 2) / population),
                    (unsigned char)((sums[2] + population / 2) / population)};
    color_format(result, color);
    free(bins);
    return true;
}
