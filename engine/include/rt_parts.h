/*
 * rt_parts.h - the theme vocabulary: parts, states and built-in glyphs.
 *
 * Recognisers translate what USER draws into (part, state); themes say how
 * each (part, state) looks. The names in rt_parts.c are the section names
 * used in THEME.INI ("<Part>.<State>"), so they are part of the theme format
 * and must not be renamed.
 */
#ifndef RT_PARTS_H
#define RT_PARTS_H

enum RtPart {
    RT_PART_FRAME,          /* window frame ring; request carries a hole (inner rect) */
    RT_PART_DLGFRAME,       /* dialog frame (WS_DLGFRAME / modal frame), with hole */
    RT_PART_CAPTION,        /* caption background */
    RT_PART_CAPTIONTEXT,    /* caption title */
    RT_PART_BTNCLOSE,
    RT_PART_BTNMIN,
    RT_PART_BTNMAX,
    RT_PART_BTNRESTORE,
    RT_PART_MENUBAR,        /* menu bar background */
    RT_PART_MENUBARITEM,    /* menu bar item background */
    RT_PART_MENUBARTEXT,
    RT_PART_MENU,           /* popup menu window (border + background) */
    RT_PART_MENUITEM,       /* popup item background */
    RT_PART_MENUTEXT,
    RT_PART_MENUSEPARATOR,
    RT_PART_MENUCHECK,
    RT_PART_MENUSUBARROW,
    RT_PART_PUSHBUTTON,
    RT_PART_PUSHBUTTONTEXT,
    RT_PART_FOCUS,          /* keyboard focus indicator */
    RT_PART_CHECKBOX,
    RT_PART_RADIO,
    RT_PART_GROUPBOX,
    RT_PART_STATICTEXT,
    RT_PART_FIELD,          /* edit / list box / combo box border, with hole */
    RT_PART_COMBOBUTTON,
    RT_PART_SCROLLUP,
    RT_PART_SCROLLDOWN,
    RT_PART_SCROLLLEFT,
    RT_PART_SCROLLRIGHT,
    RT_PART_SCROLLTHUMBV,
    RT_PART_SCROLLTHUMBH,
    RT_PART_SCROLLTRACKV,
    RT_PART_SCROLLTRACKH,
    RT_PART_SCROLLSIZEBOX,
    RT_PART_DIALOG,         /* dialog / static background */
    RT_PART_MDICLIENT,
    RT_PART_DESKTOP,
    RT_PART_ICONTITLE,
    RT_PART_COUNT
};

enum RtState {
    RT_ST_NORMAL,
    RT_ST_PRESSED,
    RT_ST_DISABLED,
    RT_ST_ACTIVE,
    RT_ST_INACTIVE,
    RT_ST_SELECTED,
    RT_ST_HOT,
    RT_ST_DEFAULT,          /* default push button */
    RT_ST_DEFAULTPRESSED,
    RT_ST_CHECKED,
    RT_ST_CHECKEDPRESSED,
    RT_ST_CHECKEDDISABLED,
    RT_ST_MIXED,
    RT_ST_MIXEDPRESSED,
    RT_ST_MIXEDDISABLED,
    RT_ST_COUNT
};

/* Built-in vector glyphs (render/glyphs.c). */
enum RtGlyph {
    RT_GLYPH_NONE,
    RT_GLYPH_CLOSE,
    RT_GLYPH_MIN,
    RT_GLYPH_MAX,
    RT_GLYPH_RESTORE,
    RT_GLYPH_ARROWUP,
    RT_GLYPH_ARROWDOWN,
    RT_GLYPH_ARROWLEFT,
    RT_GLYPH_ARROWRIGHT,
    RT_GLYPH_CHECK,         /* check box mark */
    RT_GLYPH_MENUCHECK,     /* menu check mark */
    RT_GLYPH_SUBARROW,      /* menu sub-menu arrow */
    RT_GLYPH_RADIODOT,
    RT_GLYPH_RADIOFILL,     /* interior disc of a radio button */
    RT_GLYPH_RADIOOUTER,    /* outer ring: colour 1 = top-left arc, colour 2 = bottom-right arc */
    RT_GLYPH_RADIOINNER,    /* inner ring, same colour convention */
    RT_GLYPH_SIZEGRIP,
    RT_GLYPH_COUNT
};

extern const char *const rt_part_names[RT_PART_COUNT];
extern const char *const rt_state_names[RT_ST_COUNT];
extern const char *const rt_glyph_names[RT_GLYPH_COUNT];

int rt_part_from_name(const char *name, int len);   /* -1 if unknown */
int rt_state_from_name(const char *name);           /* -1 if unknown */
int rt_glyph_from_name(const char *name);           /* 0 if unknown */

/* Next state to try when a theme does not define 'state'; RT_ST_NORMAL ends
 * the chain. */
int rt_state_fallback(int state);

#endif
