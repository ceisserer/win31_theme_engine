/* parts.c - names of parts, states and glyphs as used in THEME.INI. */
#include "rt_types.h"
#include "rt_parts.h"

const char *const rt_part_names[RT_PART_COUNT] = {
    "Frame", "DlgFrame", "Caption", "CaptionText",
    "BtnClose", "BtnMin", "BtnMax", "BtnRestore",
    "MenuBar", "MenuBarItem", "MenuBarText",
    "Menu", "MenuItem", "MenuText", "MenuSeparator", "MenuCheck", "MenuSubArrow",
    "PushButton", "PushButtonText", "Focus",
    "CheckBox", "Radio", "GroupBox", "StaticText",
    "Field", "ComboButton",
    "ScrollUp", "ScrollDown", "ScrollLeft", "ScrollRight",
    "ScrollThumbV", "ScrollThumbH", "ScrollTrackV", "ScrollTrackH", "ScrollSizeBox",
    "Dialog", "MDIClient", "Desktop", "IconTitle"
};

const char *const rt_state_names[RT_ST_COUNT] = {
    "Normal", "Pressed", "Disabled", "Active", "Inactive", "Selected", "Hot",
    "Default", "DefaultPressed",
    "Checked", "CheckedPressed", "CheckedDisabled",
    "Mixed", "MixedPressed", "MixedDisabled"
};

const char *const rt_glyph_names[RT_GLYPH_COUNT] = {
    "None", "Close", "Min", "Max", "Restore",
    "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight",
    "Check", "MenuCheck", "SubArrow",
    "RadioDot", "RadioFill", "RadioOuter", "RadioInner", "SizeGrip"
};

static const unsigned char state_fallback[RT_ST_COUNT] = {
    RT_ST_NORMAL,           /* Normal (end) */
    RT_ST_NORMAL,           /* Pressed */
    RT_ST_NORMAL,           /* Disabled */
    RT_ST_NORMAL,           /* Active */
    RT_ST_NORMAL,           /* Inactive */
    RT_ST_NORMAL,           /* Selected */
    RT_ST_NORMAL,           /* Hot */
    RT_ST_NORMAL,           /* Default */
    RT_ST_PRESSED,          /* DefaultPressed */
    RT_ST_NORMAL,           /* Checked */
    RT_ST_CHECKED,          /* CheckedPressed */
    RT_ST_DISABLED,         /* CheckedDisabled */
    RT_ST_CHECKED,          /* Mixed */
    RT_ST_MIXED,            /* MixedPressed */
    RT_ST_CHECKEDDISABLED   /* MixedDisabled */
};

int rt_state_fallback(int state)
{
    if (state <= 0 || state >= RT_ST_COUNT)
        return RT_ST_NORMAL;
    return state_fallback[state];
}

int rt_part_from_name(const char *name, int len)
{
    int i;
    for (i = 0; i < RT_PART_COUNT; i++) {
        if (rt_strlen(rt_part_names[i]) == len && rt_strnicmp(rt_part_names[i], name, len) == 0)
            return i;
    }
    return -1;
}

int rt_state_from_name(const char *name)
{
    int i;
    for (i = 0; i < RT_ST_COUNT; i++)
        if (rt_stricmp(rt_state_names[i], name) == 0)
            return i;
    return -1;
}

int rt_glyph_from_name(const char *name)
{
    int i;
    for (i = 1; i < RT_GLYPH_COUNT; i++)
        if (rt_stricmp(rt_glyph_names[i], name) == 0)
            return i;
    return 0;
}
