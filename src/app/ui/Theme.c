#include "app/ui/Theme.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/Color.h"
#include "platform/web/Browser.h"

/* These are DOM style property assignments, not stylesheets or parsed rules. */
typedef struct
{
    const char *elements, *property, *value;
} Style;

static const Style base[] = {
    {"html", "color-scheme", "dark"},
    {"html", "font-family", "Consolas, 'Liberation Mono', 'Courier New', monospace"},
    {"html", "font-synthesis", "none"},
    {"*", "box-sizing", "border-box"},
    {"body", "margin", "0"},
    {"body", "background", "#07131e"},
    {"body", "color", "#dce8ee"},
    {"body", "font-size", "15px"},
    {"body", "line-height", "1.6"},
    {"button, input, textarea", "font", "inherit"},
    {"button, input, textarea", "color", "inherit"},
    {"button", "cursor", "pointer"},
    {"button, a, summary", "touch-action", "manipulation"},
    {"a", "color", "#61d6fa"},
    {"a", "text-decoration", "none"},
    {".topbar", "max-width", "1180px"},
    {".topbar", "min-height", "100px"},
    {".topbar", "margin", "auto"},
    {".topbar", "padding", "20px 4px"},
    {".topbar", "display", "flex"},
    {".topbar", "align-items", "center"},
    {".topbar", "justify-content", "space-between"},
    {".topbar", "gap", "20px"},
    {".brand", "font-size", "24px"},
    {".brand", "font-weight", "700"},
    {".brand", "letter-spacing", "-.6px"},
    {".brand span", "color", "#dce8ee"},
    {".brand span", "font-weight", "400"},
    {".brand span", "margin-left", "14px"},
    {".brand span", "font-size", "18px"},
    {".mode, .button", "border", "1px solid #426278"},
    {".mode, .button", "background", "transparent"},
    {".mode, .button", "padding", "8px 14px"},
    {".mode, .button", "font-size", "13px"},
    {".mode.active", "color", "#61d6fa"},
    {".mode.active", "border-color", "#61d6fa"},
    {".page", "max-width", "1180px"},
    {".page", "margin", "0 auto 60px"},
    {".page", "background", "#0d2030"},
    {".page", "border", "1px solid #28475c"},
    {".page", "padding", "0 38px 40px"},
    {".page", "min-height", "calc(100vh - 160px)"},
    {".song-discussion", "max-width", "1180px"},
    {".song-discussion", "margin", "-60px auto 60px"},
    {".song-discussion", "padding", "30px 38px 40px"},
    {".song-discussion", "background", "#0d2030"},
    {".song-discussion", "border", "1px solid #28475c"},
    {".song-discussion", "border-top", "0"},
    {".song-discussion[hidden]", "display", "none"},
    {".back-link", "display", "inline-block"},
    {".back-link", "margin-top", "22px"},
    {".back-link", "font-size", "13px"},
    {".resume-draft", "font-size", "13px"},
    {".index-heading", "padding", "30px 0 0"},
    {".index-heading", "display", "flex"},
    {".index-heading", "align-items", "end"},
    {".index-heading", "justify-content", "space-between"},
    {".index-heading", "gap", "24px"},
    {".index-heading .field", "margin", "0"},
    {".index-heading .field", "width", "280px"},
    {".index-count", "color", "#91adbf"},
    {".index-count", "font-size", "13px"},
    {".index-count", "margin", "12px 0 20px"},
    {".index-alphabet", "display", "flex"},
    {".index-alphabet", "flex-wrap", "wrap"},
    {".index-alphabet", "gap", "6px 15px"},
    {".index-alphabet", "border-top", "1px solid #28475c"},
    {".index-alphabet", "border-bottom", "1px solid #28475c"},
    {".index-alphabet", "padding", "14px 0"},
    {"span.index-letter", "color", "#637c8c"},
    {".index-groups", "columns", "3 220px"},
    {".index-groups", "column-gap", "40px"},
    {".index-groups", "padding-top", "26px"},
    {".index-group", "break-inside", "avoid"},
    {".index-group", "margin-bottom", "30px"},
    {".index-group h2", "font-size", "22px"},
    {".index-group h2", "font-weight", "400"},
    {".index-group h2", "margin", "0 0 10px"},
    {".index-group h2", "border-bottom", "1px solid #28475c"},
    {".index-list", "list-style", "none"},
    {".index-list", "margin", "0"},
    {".index-list", "padding", "0"},
    {".index-list li", "margin", "6px 0"},
    {".index-list a", "overflow-wrap", "anywhere"},
    {".index-empty", "color", "#91adbf"},
    {".hero", "padding", "28px 0 24px"},
    {".hero", "display", "flow-root"},
    {".hero", "position", "relative"},
    {".artwork", "display", "block"},
    {".artwork", "float", "right"},
    {".artwork", "width", "160px"},
    {".artwork", "height", "160px"},
    {".artwork", "object-fit", "cover"},
    {".artwork", "margin", "0 0 20px 28px"},
    {".artwork", "border", "1px solid #28475c"},
    {"h1", "font-size", "42px"},
    {"h1", "line-height", "1.2"},
    {"h1", "margin", "0 0 4px"},
    {"h1", "font-weight", "400"},
    {"h1", "letter-spacing", "-1px"},
    {"h1", "overflow-wrap", "anywhere"},
    {".artist", "margin", "0 0 17px"},
    {".artist", "font-size", "16px"},
    {".artist", "color", "#91adbf"},
    {".metadata", "display", "flex"},
    {".metadata", "flex-wrap", "wrap"},
    {".metadata", "align-items", "center"},
    {".metadata", "gap", "8px 0"},
    {".metadata", "font-size", "12px"},
    {".metadata", "color", "#91adbf"},
    {".metadata > * + *", "border-left", "1px solid #426278"},
    {".metadata > * + *", "margin-left", "16px"},
    {".metadata > * + *", "padding-left", "16px"},
    {".metadata strong", "color", "#dce8ee"},
    {".metadata strong", "font-weight", "400"},
    {".source-link", "text-underline-offset", "3px"},
    {".reading-header", "border-bottom", "1px solid #28475c"},
    {".reading-header h2", "font-size", "22px"},
    {".reading-header h2", "font-weight", "400"},
    {".reading-header h2", "margin", "0 0 9px"},
    {".reading-grid", "display", "grid"},
    {".reading-grid", "grid-template-columns", "minmax(0, 1fr)"},
    {".reading-grid", "gap", "36px"},
    {".reading-grid", "padding-top", "24px"},
    {".reading-grid", "align-items", "start"},
    {".lyrics", "min-width", "0"},
    {".eyebrow", "color", "#91adbf"},
    {".eyebrow", "font-size", "13px"},
    {".eyebrow", "font-weight", "400"},
    {".stanza", "margin-bottom", "28px"},
    {".section-label", "margin", "0 0 10px"},
    {".section-label", "font-size", "13px"},
    {".section-label", "font-weight", "400"},
    {".section-label", "color", "#91adbf"},
    {".title-line", "background", "transparent"},
    {".link-fields", "display", "grid"},
    {".link-fields", "grid-template-columns", "1fr 2fr auto"},
    {".link-fields", "align-items", "center"},
    {".link-fields", "gap", "12px"},
    {".link-fields .field", "min-width", "0"},
    {".original-line", "opacity", ".65"},
    {".original-line.selected", "opacity", "1"},
    {".original-line.selected", "background", "#225870"},
    {".original-line.selected", "font-weight", "700"},
    {".lyric-row", "font-size", "18px"},
    {".lyric-row", "line-height", "1.9"},
    {".lyric-line, .title-line", "display", "inline"},
    {".lyric-line, .title-line", "max-width", "100%"},
    {".lyric-line, .title-line", "border", "0"},
    {".lyric-line, .title-line", "padding", "0"},
    {".lyric-line, .title-line", "margin", "0"},
    {".lyric-line, .title-line", "font", "inherit"},
    {".lyric-line, .title-line", "text-align", "left"},
    {".lyric-line, .title-line", "white-space", "pre-wrap"},
    {".lyric-line, .title-line", "overflow-wrap", "anywhere"},
    {".lyric-line, .title-line", "box-decoration-break", "clone"},
    {".annotation-range", "display", "grid"},
    {".annotation-range", "grid-template-columns", "1fr 1fr"},
    {".annotation-range", "gap", "12px"},
    {".section-editor", "margin", "14px 0"},
    {".section-editor summary", "cursor", "pointer"},
    {".section-editor summary", "color", "#61d6fa"},
    {".section-editor summary", "margin-bottom", "14px"},
    {".section-editor .field:first-of-type", "margin-top", "14px"},
    {".original-block + .field", "margin-top", "18px"},
    {".annotation-panel", "border", "1px solid #3b6278"},
    {".annotation-panel", "border-top", "2px solid #61d6fa"},
    {".annotation-panel", "padding", "20px"},
    {".annotation-panel", "background", "#102a3d"},
    {".annotation-panel", "position", "sticky"},
    {".annotation-panel", "top", "20px"},
    {".annotation-panel", "max-height", "calc(100vh - 40px)"},
    {".annotation-panel", "overflow", "auto"},
    {".annotation-panel", "scrollbar-width", "thin"},
    {".annotation-panel", "scrollbar-color", "#45677d transparent"},
    {".panel-top", "display", "flex"},
    {".panel-top", "align-items", "baseline"},
    {".panel-top", "justify-content", "space-between"},
    {".panel-top", "gap", "12px"},
    {".panel-top", "margin-bottom", "20px"},
    {".panel-top h2", "margin", "0"},
    {".panel-top h2", "font-size", "18px"},
    {".panel-top h2", "font-weight", "400"},
    {".panel-top h2", "overflow-wrap", "anywhere"},
    {".close", "background", "none"},
    {".close", "color", "#91adbf"},
    {".close", "border", "0"},
    {".close", "padding", "4px 0 4px 8px"},
    {".close", "font-size", "12px"},
    {".close", "flex-shrink", "0"},
    {".original-block", "border-left", "2px solid #4d7389"},
    {".original-block", "padding", "12px 14px"},
    {".original-block", "background", "#0b1e2e"},
    {".original-block p", "margin", "0"},
    {".original-block p", "white-space", "pre-line"},
    {".original-block p", "overflow-wrap", "anywhere"},
    {".original-block p", "font-size", "14px"},
    {".original-block p", "line-height", "1.8"},
    {".original-block + .markdown", "margin-top", "22px"},
    {".markdown", "color", "#dce8ee"},
    {".markdown", "font-size", "14px"},
    {".markdown", "line-height", "1.8"},
    {".markdown", "overflow-wrap", "anywhere"},
    {".markdown p", "margin", "0 0 14px"},
    {".markdown p:last-child", "margin-bottom", "0"},
    {".markdown h3", "font-size", "15px"},
    {".markdown h3", "margin", "18px 0 10px"},
    {".markdown blockquote", "border-left", "2px solid #558299"},
    {".markdown blockquote", "padding-left", "13px"},
    {".markdown blockquote", "margin", "16px 0"},
    {".markdown blockquote", "color", "#adc5d2"},
    {".markdown ul, .markdown ol", "padding-left", "22px"},
    {".markdown li", "margin-bottom", "5px"},
    {".markdown a", "text-decoration", "underline"},
    {".markdown a", "text-underline-offset", "3px"},
    {".markdown a:not([href])", "color", "inherit"},
    {".markdown a:not([href])", "text-decoration", "none"},
    {".markdown pre", "padding", "12px"},
    {".markdown pre", "background", "#081926"},
    {".markdown pre", "overflow", "auto"},
    {".markdown code", "font", "inherit"},
    {".markdown code", "color", "#61d6fa"},
    {".text-button", "background", "none"},
    {".text-button", "border", "0"},
    {".text-button", "color", "#61d6fa"},
    {".text-button", "padding", "4px 0"},
    {".text-button", "font-size", "12px"},
    {".sheet-backdrop, .sheet-handle", "display", "none"},
    {".editor-meta", "border", "1px solid #3b6278"},
    {".editor-meta", "background", "#0a1b29"},
    {".editor-meta", "padding", "18px 20px"},
    {".editor-meta", "margin-bottom", "28px"},
    {".editor-toolbar", "display", "flex"},
    {".editor-toolbar", "align-items", "center"},
    {".editor-toolbar", "justify-content", "space-between"},
    {".editor-toolbar", "gap", "20px"},
    {".editor-toolbar p", "margin", "0"},
    {".editor-toolbar p", "color", "#91adbf"},
    {".editor-toolbar p", "font-size", "12px"},
    {".toolbar-buttons", "display", "flex"},
    {".toolbar-buttons", "gap", "10px"},
    {".toolbar-buttons", "flex-shrink", "0"},
    {".primary", "color", "#06121c"},
    {".primary", "background", "#61d6fa"},
    {".primary", "border-color", "#61d6fa"},
    {".secondary", "color", "#61d6fa"},
    {".danger", "color", "#e9a7a7"},
    {".danger", "border-color", "#7d505d"},
    {".editor-meta summary", "cursor", "pointer"},
    {".editor-meta summary", "padding-top", "15px"},
    {".editor-meta summary", "font-size", "13px"},
    {".editor-meta summary", "color", "#61d6fa"},
    {".metadata-fields", "display", "grid"},
    {".metadata-fields", "grid-template-columns", "1fr 1fr"},
    {".metadata-fields", "gap", "0 20px"},
    {".metadata-fields", "margin-top", "20px"},
    {".field", "display", "block"},
    {".field", "margin-bottom", "18px"},
    {".field > span", "display", "block"},
    {".field > span", "font-size", "12px"},
    {".field > span", "color", "#91adbf"},
    {".field > span", "margin-bottom", "7px"},
    {".field input, .field textarea", "width", "100%"},
    {".field input, .field textarea", "border", "1px solid #3b6278"},
    {".field input, .field textarea", "background", "#071723"},
    {".field input, .field textarea", "padding", "10px"},
    {".field input, .field textarea", "font-size", "14px"},
    {".field input, .field textarea", "line-height", "1.6"},
    {".field input, .field textarea", "border-radius", "0"},
    {".field textarea", "min-height", "110px"},
    {".field textarea", "resize", "vertical"},
    {"#annotation", "min-height", "190px"},
    {".preview-label", "display", "block"},
    {".preview-label", "margin-bottom", "12px"},
    {".panel-buttons", "display", "flex"},
    {".panel-buttons", "flex-wrap", "wrap"},
    {".panel-buttons", "gap", "10px"},
    {".panel-buttons", "border-top", "1px solid #28475c"},
    {".panel-buttons", "padding-top", "18px"},
    {".panel-buttons", "margin-top", "20px"},
    {".panel-buttons .button", "padding", "8px 10px"},
    {".panel-buttons .button", "font-size", "12px"},
    {".add-section", "background", "transparent"},
    {".add-section", "border", "1px dashed #426278"},
    {".add-section", "color", "#61d6fa"},
    {".add-section", "padding", "10px 16px"},
    {".add-section", "font-size", "13px"},
    {".add-section", "margin", "6px 0 0 16px"},
    {"#status", "transition", "opacity 200ms ease"},
    {"#status:not([data-toast-state=fading])", "opacity", "1"},
    {"#status[data-toast-state=fading]", "opacity", "0"},
    {"#status", "position", "fixed"},
    {"#status", "bottom", "16px"},
    {"#status", "left", "50%"},
    {"#status", "transform", "translateX(-50%)"},
    {"#status", "z-index", "20"},
    {"#status", "background", "#14374b"},
    {"#status", "border", "1px solid #528299"},
    {"#status", "width", "max-content"},
    {"#status", "max-width", "calc(100% - 32px)"},
    {"#status", "font-size", "12px"},
    {"#status:empty", "display", "none"},
    {"#status:not(:empty)", "padding", "10px 16px"},
    {".loading", "padding", "10vh 10vw"},
    {".loading", "color", "#61d6fa"},
};
/* The title note overlays the header. Its host stays in the header on mobile,
 * where the existing fixed sheet styles take it out of the document flow. */
static const Style desktop_title[] = {
    {".title-panel-host", "position", "absolute"},
    {".title-panel-host", "top", "28px"},
    {".title-panel-host", "right", "0"},
    {".title-panel-host", "width", "clamp(280px, 30vw, 340px)"},
    {".title-panel-host", "z-index", "5"},
    {".title-panel-host .annotation-panel", "position", "relative"},
    {".title-panel-host .annotation-panel", "top", "0"},
    {".title-panel-host .annotation-panel", "max-height", "min(640px, calc(100dvh - 200px))"},
    {".title-panel-host .annotation-panel", "box-shadow", "0 12px 32px #00000066"},
};

static const Style medium[] = {
    {".topbar", "margin", "0 28px"},
    {".page", "margin", "0 28px 40px"},
    {".song-discussion", "margin", "-40px 28px 40px"},
    {".song-discussion", "padding", "24px 28px 32px"},
    {".page", "padding", "0 28px 32px"},
    {".reading-grid", "gap", "22px"},
};
static const Style mobile[] = {
    {".link-fields", "grid-template-columns", "1fr"},
    {".artwork", "display", "none"},
    {".index-heading", "display", "block"},
    {".index-heading .field", "width", "100%"},
    {".index-heading .field", "margin-top", "22px"},
    {".index-groups", "columns", "1"},
    {".lyric-row", "font-size", "16px"},
    {".topbar", "min-height", "84px"},
    {".topbar", "margin", "0 18px"},
    {".topbar", "gap", "10px"},
    {".brand", "font-size", "18px"},
    {".brand", "letter-spacing", "-.5px"},
    {".brand span", "display", "block"},
    {".brand span", "margin", "0"},
    {".brand span", "font-size", "13px"},
    {".mode", "font-size", "12px"},
    {".mode", "padding", "7px 9px"},
    {".page", "margin", "0 10px 20px"},
    {".song-discussion", "margin", "-20px 10px 20px"},
    {".song-discussion", "padding", "20px 16px 26px"},
    {".page", "padding", "0 16px 26px"},
    {".page", "min-height", "calc(100vh - 104px)"},
    {".hero", "padding", "23px 0"},
    {"h1", "font-size", "34px"},
    {".artist", "font-size", "14px"},
    {".artist", "margin-bottom", "15px"},
    {".metadata", "font-size", "11px"},
    {".metadata > * + *", "margin-left", "10px"},
    {".metadata > * + *", "padding-left", "10px"},
    {".reading-header h2", "font-size", "20px"},
    {".reading-grid", "display", "block"},
    {".reading-grid", "padding-top", "15px"},
    {".eyebrow", "font-size", "12px"},
    {".annotation-panel", "position", "fixed"},
    {".annotation-panel", "z-index", "11"},
    {".annotation-panel", "left", "0"},
    {".annotation-panel", "right", "0"},
    {".annotation-panel", "bottom", "0"},
    {".annotation-panel", "top", "auto"},
    {".annotation-panel", "max-height", "84dvh"},
    {".annotation-panel", "padding", "12px 22px 60px"},
    {".annotation-panel", "overscroll-behavior", "contain"},
    {".sheet-backdrop", "display", "block"},
    {".sheet-backdrop", "position", "fixed"},
    {".sheet-backdrop", "z-index", "10"},
    {".sheet-backdrop", "inset", "0"},
    {".sheet-backdrop", "background", "#020910c9"},
    {".sheet-backdrop", "border", "0"},
    {".sheet-handle", "display", "block"},
    {".sheet-handle", "width", "34px"},
    {".sheet-handle", "height", "3px"},
    {".sheet-handle", "background", "#5e8297"},
    {".sheet-handle", "margin", "0 auto 15px"},
    {".panel-top", "align-items", "center"},
    {".panel-top", "margin-bottom", "18px"},
    {".close", "padding", "9px 0 9px 12px"},
    {".editor-meta", "padding", "15px"},
    {".editor-toolbar", "display", "block"},
    {".toolbar-buttons", "margin-top", "13px"},
    {".button", "font-size", "12px"},
    {".button", "padding", "8px 10px"},
    {".metadata-fields", "grid-template-columns", "1fr"},
    {".field input, .field textarea", "font-size", "16px"},
    {"#status", "font-size", "11px"},
    {"#status", "bottom", "10px"},
};

/* Preserve the default palette when no song color is authored. All generated
 * shades keep text light and surfaces dark, even for very bright artwork. */
typedef struct
{
    const char* original;
    unsigned amount;
    int light;
} Shade;

static const Shade shades[] = {
    {"#07131e", 12, 0}, {"#071723", 18, 0}, {"#081926", 18, 0}, {"#0a1b29", 18, 0},
    {"#0b1e2e", 18, 0}, {"#0d2030", 22, 0}, {"#102a3d", 27, 0}, {"#14374b", 27, 0},
    {"#16384b", 27, 0}, {"#17374c", 27, 0}, {"#225870", 38, 0}, {"#28475c", 35, 0},
    {"#3b6278", 55, 0}, {"#426278", 55, 0}, {"#45677d", 55, 0}, {"#4d7389", 55, 0},
    {"#528299", 55, 0}, {"#558299", 55, 0}, {"#5e8297", 55, 0}, {"#61d6fa", 35, 1},
    {"#a5eaff", 20, 1},
};

static void
themed_style(const char* color, const char* selector, const char* property, const char* value)
{
    Color seed;
    if (color_parse(color, &seed))
    {
        for (size_t i = 0; i < sizeof(shades) / sizeof(*shades); ++i)
        {
            const char* match = strstr(value, shades[i].original);
            if (match)
            {
                char replacement[8], result[256];
                Color neutral = shades[i].light ? (Color){255, 255, 255} : (Color){5, 10, 15};
                color_mix(seed, neutral, shades[i].amount, replacement);
                size_t prefix = (size_t)(match - value);
                size_t length = strlen(value);
                if (length < sizeof(result))
                {
                    memcpy(result, value, length + 1);
                    memcpy(result + prefix, replacement, 7);
                    ui_style(selector, property, result);
                    return;
                }
            }
        }
    }
    ui_style(selector, property, value);
}

static void apply(const char* color, const Style* styles, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        themed_style(color, styles[i].elements, styles[i].property, styles[i].value);
    }
}

#define APPLY(styles) apply(color, styles, sizeof(styles) / sizeof(*styles))

void theme_interaction(const char* color)
{
    themed_style(color, ".close", "color", "#91adbf");
    themed_style(color, ".close:hover", "color", "#61d6fa");
    themed_style(color, ".lyric-line, .title-line", "background", "transparent");
    themed_style(color, ".lyric-line.annotated, .title-line.annotated", "background", "#16384b");
    themed_style(color, ".lyric-line:hover, .title-line:hover", "background", "#225870");
    themed_style(color, ".lyric-line.selected, .title-line.selected", "background", "#225870");
    themed_style(color, "a", "text-decoration", "none");
    themed_style(color, ".markdown a[href], .source-link[href]", "text-decoration", "underline");
    themed_style(color, "a:hover", "text-decoration", "underline");
    themed_style(color, ".brand:hover", "text-decoration", "none");
    themed_style(color, ".mode,.button", "background", "transparent");
    themed_style(color, ".mode,.button", "border-color", "#426278");
    themed_style(color, ".mode.active", "border-color", "#61d6fa");
    themed_style(color, ".danger", "border-color", "#7d505d");
    themed_style(color, ".primary", "background", "#61d6fa");
    themed_style(color, ".primary", "border-color", "#61d6fa");
    themed_style(color, ".mode:hover,.button:hover", "background", "#17374c");
    themed_style(color, ".mode:hover,.button:hover", "border-color", "#61d6fa");
    themed_style(color, ".primary:hover", "background", "#a5eaff");
    themed_style(color, "button,a,input,textarea,summary", "outline", "");
    themed_style(color, "button,a,input,textarea,summary", "outline-offset", "");
    themed_style(color, ":focus-visible", "outline", "2px solid #61d6fa");
    themed_style(color, ":focus-visible", "outline-offset", "3px");
    themed_style(color, "#panel", "outline", "none");
}

static void layout_reader(int viewport_width)
{
    if (viewport_width <= 760 || !ui_count(".lyric-line"))
    {
        return;
    }
    /* Measure every line without wrapping, including verses below the fold.
     * Restore the normal inline presentation before the browser paints. */
    ui_style(".lyric-line", "display", "inline-block");
    ui_style(".lyric-line", "white-space", "pre");
    ui_style(".lyric-line", "max-width", "none");
    double longest_line = ui_max_width(".lyric-line");
    ui_style(".lyric-line", "display", "inline");
    ui_style(".lyric-line", "white-space", "pre-wrap");
    ui_style(".lyric-line", "max-width", "100%");
    int gap = viewport_width <= 1240 ? 22 : 36;
    char columns[160];
    /* Very long lyrics wrap rather than reducing the panel below 280px.
     * The outer min keeps the half-width cap even on a narrow desktop. */
    snprintf(columns,
             sizeof(columns),
             "minmax(0, 1fr) min(50%%, max(280px, calc(100%% - %.3fpx - %dpx)))",
             longest_line,
             gap);
    ui_style(".reading-grid.has-panel", "grid-template-columns", columns);
}

void theme_apply(const char* color)
{
    int width = ui_width();
    /* Giscus owns its subtree, including the iframe's dynamically sized height. */
    ui_clear_styles("html,body,body *:not(.giscus, .giscus *)");
    APPLY(base);
    if (width > 760)
    {
        APPLY(desktop_title);
    }
    if (width <= 1240)
    {
        APPLY(medium);
    }
    if (width <= 760)
    {
        APPLY(mobile);
    }
    int sheet = width <= 760 && ui_count("#panel") > 0;
    ui_style("body", "overflow", sheet ? "hidden" : "visible");
    ui_attribute("#panel", "role", sheet ? "dialog" : "complementary");
    ui_attribute("#panel", "aria-modal", sheet ? "true" : "false");
    Color seed;
    char background[8] = "#07131e";
    if (color_parse(color, &seed))
    {
        color_mix(seed, (Color){5, 10, 15}, 12, background);
    }
    ui_attribute("meta[name=theme-color]", "content", background);
    theme_interaction(color);
    layout_reader(width);
}
