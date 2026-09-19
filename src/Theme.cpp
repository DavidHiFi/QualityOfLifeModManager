#include "Theme.h"

#include <QApplication>
#include <algorithm>
#include <QCoreApplication>
#include <QFile>
#include <QMap>
#include <QPainter>
#include <QPixmap>
#include <QTextStream>

namespace {

using Palette = QMap<QString, QString>;

// Tokens comuns a todos os temas (tamanhos, fontes de fallback).
Palette base()
{
    Palette p;
    p["FONT"]        = "\"Inter\", \"Segoe UI\", sans-serif";
    p["CHECK_ICON"]  = ":/icons/check.svg";
    p["R"]           = "8px";
    p["R_SM"]        = "6px";
    p["R_XS"]        = "5px";
    p["R_ROUND"]     = "8px";
    p["R_ARROW"]     = "19px";
    p["R_DOT"]       = "4px";
    return p;
}

Palette nocturne()
{
    Palette p = base();
    p["BG"]            = "#161826";
    p["BG_DEEP"]       = "#101120";
    p["HEADER_TOP"]    = "#1b1d2e";
    p["HEADER_BOT"]    = "#161826";
    p["SURFACE"]       = "#1e2032";
    p["SURFACE_2"]     = "#191b29";
    p["SURFACE_HI"]    = "#1c1e2e";
    p["FIELD"]         = "#131426";
    p["FIELD_DISABLED"]= "#15161f";
    p["LINE"]          = "#2b2e42";
    p["LINE_SOFT"]     = "#22243a";
    p["LINE_STRONG"]   = "#3a3d57";
    p["LINE_CARD"]     = "#272a3e";
    p["TEXT"]          = "#e9e9ed";
    p["TEXT_STRONG"]   = "#f4f4f7";
    p["TEXT_DIM"]      = "#8688a1";
    p["TEXT_FAINT"]    = "#6b6c86";
    p["NAV_TEXT"]      = "#b6b7ce";
    p["ACCENT"]        = "#9184d9";
    p["ACCENT_DARK"]   = "#7a6cc9";
    p["ACCENT_LIGHT"]  = "#a79de2";
    p["ACCENT_SOFT"]   = "#b6acea";
    p["ACCENT_TEXT"]   = "#14121f";
    p["TINT1"]         = "rgba(145,132,217,0.14)";
    p["TINT2"]         = "rgba(145,132,217,0.24)";
    p["TINT3"]         = "rgba(145,132,217,0.34)";
    p["TINT_BORDER"]   = "rgba(145,132,217,0.45)";
    p["ON_SELECT"]     = "#ffffff";
    p["BTN"]           = "#212436";
    p["BTN_HOVER"]     = "#282c42";
    p["BTN_PRESS"]     = "#1b1d2c";
    p["BTN_LINE"]      = "#2d3045";
    p["BTN_TEXT"]      = "#e2e3ee";
    p["DISABLED_BG"]   = "#191b28";
    p["DISABLED_TEXT"] = "#5b5c72";
    p["DISABLED_LINE"] = "#24263a";
    p["SCROLL"]        = "#2e3149";
    p["SCROLL_HOVER"]  = "#3f4260";
    p["DANGER_TINT"]   = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"]  = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"]   = "#7d454b";
    p["DANGER_TEXT"]   = "#e2a3a8";
    p["DANGER_TEXT_HI"]= "#f3c3c6";
    p["OK"]            = "#7ddea0";
    p["WARN"]          = "#f0c36d";
    p["TOOLTIP_BG"]    = "#1e2032";
    p["HERO_TEXT"]     = "#ffffff";
    p["HERO_SUB"]      = "#c5c6da";
    p["SIDEBAR_BG"]    = "#101120";
    p["ICON"]          = "#c8c9dc";
    return p;
}

// Classic Things: cantos retos, molduras de 1px, metal + ambar.
Palette classicDark()
{
    Palette p = base();
    p["FONT"]          = "\"Segoe UI\", \"Tahoma\", sans-serif";
    p["BG"]            = "#1b1b1b";
    p["BG_DEEP"]       = "#121212";
    p["HEADER_TOP"]    = "#272727";
    p["HEADER_BOT"]    = "#1b1b1b";
    p["SURFACE"]       = "#242424";
    p["SURFACE_2"]     = "#1f1f1f";
    p["SURFACE_HI"]    = "#2f2f2f";
    p["FIELD"]         = "#141414";
    p["FIELD_DISABLED"]= "#1a1a1a";
    p["LINE"]          = "#3d3d3d";
    p["LINE_SOFT"]     = "#303030";
    p["LINE_STRONG"]   = "#5c5c5c";
    p["LINE_CARD"]     = "#3a3a3a";
    p["TEXT"]          = "#e6e4e0";
    p["TEXT_STRONG"]   = "#ffffff";
    p["TEXT_DIM"]      = "#a29c93";
    p["TEXT_FAINT"]    = "#837d74";
    p["NAV_TEXT"]      = "#c9c5be";
    p["ACCENT"]        = "#e08b22";
    p["ACCENT_DARK"]   = "#b26a0f";
    p["ACCENT_LIGHT"]  = "#f2ae55";
    p["ACCENT_SOFT"]   = "#f0b160";
    p["ACCENT_TEXT"]   = "#1a1206";
    p["TINT1"]         = "rgba(224,139,34,0.14)";
    p["TINT2"]         = "rgba(224,139,34,0.26)";
    p["TINT3"]         = "rgba(224,139,34,0.38)";
    p["TINT_BORDER"]   = "rgba(224,139,34,0.55)";
    p["ON_SELECT"]     = "#ffffff";
    p["BTN"]           = "#2c2c2c";
    p["BTN_HOVER"]     = "#3a3a3a";
    p["BTN_PRESS"]     = "#202020";
    p["BTN_LINE"]      = "#4b4b4b";
    p["BTN_TEXT"]      = "#ebe9e5";
    p["DISABLED_BG"]   = "#202020";
    p["DISABLED_TEXT"] = "#6f6a63";
    p["DISABLED_LINE"] = "#2e2e2e";
    p["SCROLL"]        = "#4a4a4a";
    p["SCROLL_HOVER"]  = "#666666";
    p["DANGER_TINT"]   = "rgba(198,80,80,0.12)";
    p["DANGER_TINT2"]  = "rgba(198,80,80,0.24)";
    p["DANGER_LINE"]   = "#8a4040";
    p["DANGER_TEXT"]   = "#e5a1a1";
    p["DANGER_TEXT_HI"]= "#f5c9c9";
    p["OK"]            = "#7ec98c";
    p["WARN"]          = "#e0b64a";
    p["TOOLTIP_BG"]    = "#2c2c2c";
    p["HERO_TEXT"]     = "#ffffff";
    p["HERO_SUB"]      = "#d3cfc8";
    p["SIDEBAR_BG"]    = "#232323";
    p["ICON"]          = "#d8d4cd";
    p["R"]             = "0px";
    p["R_SM"]          = "0px";
    p["R_XS"]          = "0px";
    p["R_ROUND"]       = "0px";
    p["R_ARROW"]       = "0px";
    p["R_DOT"]         = "0px";
    return p;
}

Palette classicLight()
{
    Palette p = classicDark();
    p["CHECK_ICON"]    = ":/icons/check_light.svg";
    p["BG"]            = "#ecebe7";
    p["BG_DEEP"]       = "#dddbd5";
    p["HEADER_TOP"]    = "#f7f6f3";
    p["HEADER_BOT"]    = "#e9e7e2";
    p["SURFACE"]       = "#ffffff";
    p["SURFACE_2"]     = "#f4f2ee";
    p["SURFACE_HI"]    = "#e5e2db";
    p["FIELD"]         = "#ffffff";
    p["FIELD_DISABLED"]= "#eeece8";
    p["LINE"]          = "#b4afa6";
    p["LINE_SOFT"]     = "#cdc9c1";
    p["LINE_STRONG"]   = "#89847b";
    p["LINE_CARD"]     = "#c2bdb4";
    p["TEXT"]          = "#1e1c19";
    p["TEXT_STRONG"]   = "#000000";
    p["TEXT_DIM"]      = "#615c54";
    p["TEXT_FAINT"]    = "#7b756c";
    p["NAV_TEXT"]      = "#2b2823";
    p["ACCENT"]        = "#a45e06";
    p["ACCENT_DARK"]   = "#7d4704";
    p["ACCENT_LIGHT"]  = "#c8790f";
    p["ACCENT_SOFT"]   = "#8a5205";
    p["ACCENT_TEXT"]   = "#ffffff";
    p["TINT1"]         = "rgba(164,94,6,0.10)";
    p["TINT2"]         = "rgba(164,94,6,0.20)";
    p["TINT3"]         = "rgba(164,94,6,0.30)";
    p["TINT_BORDER"]   = "rgba(164,94,6,0.55)";
    p["ON_SELECT"]     = "#1e1c19";
    p["BTN"]           = "#e4e1db";
    p["BTN_HOVER"]     = "#f1efeb";
    p["BTN_PRESS"]     = "#d2cec6";
    p["BTN_LINE"]      = "#a9a399";
    p["BTN_TEXT"]      = "#1e1c19";
    p["DISABLED_BG"]   = "#e8e6e1";
    p["DISABLED_TEXT"] = "#9b958c";
    p["DISABLED_LINE"] = "#cdc9c1";
    p["SCROLL"]        = "#b8b3aa";
    p["SCROLL_HOVER"]  = "#948e86";
    p["DANGER_TINT"]   = "rgba(176,52,52,0.10)";
    p["DANGER_TINT2"]  = "rgba(176,52,52,0.20)";
    p["DANGER_LINE"]   = "#a44a4a";
    p["DANGER_TEXT"]   = "#8c2020";
    p["DANGER_TEXT_HI"]= "#6d1414";
    p["OK"]            = "#1f7a37";
    p["WARN"]          = "#8a6100";
    p["TOOLTIP_BG"]    = "#ffffff";
    p["HERO_TEXT"]     = "#ffffff";
    p["HERO_SUB"]      = "#e8e6e1";
    p["SIDEBAR_BG"]    = "#e0ddd5";
    p["ICON"]          = "#3b372f";
    return p;
}

// Classic Dark: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_qol_classic()
{
    Palette p = base();
    p["BG"              ] = "#1A1B20";
    p["BG_DEEP"         ] = "#141518";
    p["HEADER_TOP"      ] = "#212227";
    p["HEADER_BOT"      ] = "#1A1B20";
    p["SURFACE"         ] = "#23252B";
    p["SURFACE_2"       ] = "#1e2026";
    p["SURFACE_HI"      ] = "#292C33";
    p["FIELD"           ] = "#141518";
    p["FIELD_DISABLED"  ] = "#17181c";
    p["LINE"            ] = "#3A3E47";
    p["LINE_SOFT"       ] = "#1f2127";
    p["LINE_STRONG"     ] = "#3A3E47";
    p["LINE_CARD"       ] = "#23252b";
    p["TEXT"            ] = "#e4e6eb";
    p["TEXT_STRONG"     ] = "#E8EAEE";
    p["TEXT_DIM"        ] = "#9AA0AC";
    p["TEXT_FAINT"      ] = "#7A808B";
    p["NAV_TEXT"        ] = "#B2B8C4";
    p["ACCENT"          ] = "#4F9CF9";
    p["ACCENT_DARK"     ] = "#4180cc";
    p["ACCENT_LIGHT"    ] = "#6CB0FF";
    p["ACCENT_SOFT"     ] = "#82bcff";
    p["ACCENT_TEXT"     ] = "#0C1624";
    p["TINT1"           ] = "rgba(79,156,249,0.14)";
    p["TINT2"           ] = "rgba(79,156,249,0.24)";
    p["TINT3"           ] = "rgba(79,156,249,0.34)";
    p["TINT_BORDER"     ] = "rgba(79,156,249,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#2F323A";
    p["BTN_HOVER"       ] = "#393D46";
    p["BTN_PRESS"       ] = "#24262c";
    p["BTN_LINE"        ] = "#454951";
    p["BTN_TEXT"        ] = "#e3e5ea";
    p["DISABLED_BG"     ] = "#1e2026";
    p["DISABLED_TEXT"   ] = "#626770";
    p["DISABLED_LINE"   ] = "#1e2026";
    p["SCROLL"          ] = "#353941";
    p["SCROLL_HOVER"    ] = "#585b63";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#6DCD8A";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#23252B";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#15161A";
    p["ICON"            ] = "#B2B8C4";
    return p;
}

// OLED Black: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_oled()
{
    Palette p = base();
    p["BG"              ] = "#040406";
    p["BG_DEEP"         ] = "#000000";
    p["HEADER_TOP"      ] = "#0c0c0d";
    p["HEADER_BOT"      ] = "#040406";
    p["SURFACE"         ] = "#0C0C0F";
    p["SURFACE_2"       ] = "#08080a";
    p["SURFACE_HI"      ] = "#121216";
    p["FIELD"           ] = "#000000";
    p["FIELD_DISABLED"  ] = "#020203";
    p["LINE"            ] = "#1E1E24";
    p["LINE_SOFT"       ] = "#09090b";
    p["LINE_STRONG"     ] = "#1E1E24";
    p["LINE_CARD"       ] = "#0c0c0f";
    p["TEXT"            ] = "#e4e7eb";
    p["TEXT_STRONG"     ] = "#E9EBEF";
    p["TEXT_DIM"        ] = "#969CA8";
    p["TEXT_FAINT"      ] = "#787E8A";
    p["NAV_TEXT"        ] = "#B0B6C2";
    p["ACCENT"          ] = "#2DD4BF";
    p["ACCENT_DARK"     ] = "#25ae9d";
    p["ACCENT_LIGHT"    ] = "#5EEAD4";
    p["ACCENT_SOFT"     ] = "#76edda";
    p["ACCENT_TEXT"     ] = "#041A18";
    p["TINT1"           ] = "rgba(45,212,191,0.14)";
    p["TINT2"           ] = "rgba(45,212,191,0.24)";
    p["TINT3"           ] = "rgba(45,212,191,0.34)";
    p["TINT_BORDER"     ] = "rgba(45,212,191,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#18181D";
    p["BTN_HOVER"       ] = "#222228";
    p["BTN_PRESS"       ] = "#0e0e11";
    p["BTN_LINE"        ] = "#2f2f35";
    p["BTN_TEXT"        ] = "#e3e6ea";
    p["DISABLED_BG"     ] = "#08080a";
    p["DISABLED_TEXT"   ] = "#5b6069";
    p["DISABLED_LINE"   ] = "#08080a";
    p["SCROLL"          ] = "#1a1a20";
    p["SCROLL_HOVER"    ] = "#404045";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#6EE79F";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#0C0C0F";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#000000";
    p["ICON"            ] = "#B0B6C2";
    return p;
}

// Catppuccin Mocha: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_mocha()
{
    Palette p = base();
    p["BG"              ] = "#1E1E2E";
    p["BG_DEEP"         ] = "#11111B";
    p["HEADER_TOP"      ] = "#252534";
    p["HEADER_BOT"      ] = "#1E1E2E";
    p["SURFACE"         ] = "#313244";
    p["SURFACE_2"       ] = "#282839";
    p["SURFACE_HI"      ] = "#3A3C52";
    p["FIELD"           ] = "#11111B";
    p["FIELD_DISABLED"  ] = "#181824";
    p["LINE"            ] = "#45475A";
    p["LINE_SOFT"       ] = "#292a3b";
    p["LINE_STRONG"     ] = "#45475A";
    p["LINE_CARD"       ] = "#313244";
    p["TEXT"            ] = "#cad3f0";
    p["TEXT_STRONG"     ] = "#CDD6F4";
    p["TEXT_DIM"        ] = "#9399B2";
    p["TEXT_FAINT"      ] = "#6C7086";
    p["NAV_TEXT"        ] = "#A6ADC8";
    p["ACCENT"          ] = "#94E2D5";
    p["ACCENT_DARK"     ] = "#79b9af";
    p["ACCENT_LIGHT"    ] = "#89DCEB";
    p["ACCENT_SOFT"     ] = "#9be1ee";
    p["ACCENT_TEXT"     ] = "#11111B";
    p["TINT1"           ] = "rgba(148,226,213,0.14)";
    p["TINT2"           ] = "rgba(148,226,213,0.24)";
    p["TINT3"           ] = "rgba(148,226,213,0.34)";
    p["TINT_BORDER"     ] = "rgba(148,226,213,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#45475A";
    p["BTN_HOVER"       ] = "#585B70";
    p["BTN_PRESS"       ] = "#303141";
    p["BTN_LINE"        ] = "#626579";
    p["BTN_TEXT"        ] = "#c9d2f0";
    p["DISABLED_BG"     ] = "#282839";
    p["DISABLED_TEXT"   ] = "#585c70";
    p["DISABLED_LINE"   ] = "#282839";
    p["SCROLL"          ] = "#3f4153";
    p["SCROLL_HOVER"    ] = "#616373";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#A6E3A1";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#313244";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#181825";
    p["ICON"            ] = "#A6ADC8";
    return p;
}

// Catppuccin Macchiato: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_macchiato()
{
    Palette p = base();
    p["BG"              ] = "#24273A";
    p["BG_DEEP"         ] = "#181926";
    p["HEADER_TOP"      ] = "#2b2d40";
    p["HEADER_BOT"      ] = "#24273A";
    p["SURFACE"         ] = "#363A4F";
    p["SURFACE_2"       ] = "#2d3044";
    p["SURFACE_HI"      ] = "#40445C";
    p["FIELD"           ] = "#181926";
    p["FIELD_DISABLED"  ] = "#1e2030";
    p["LINE"            ] = "#494E66";
    p["LINE_SOFT"       ] = "#2f3247";
    p["LINE_STRONG"     ] = "#494E66";
    p["LINE_CARD"       ] = "#363a4f";
    p["TEXT"            ] = "#c7d0f2";
    p["TEXT_STRONG"     ] = "#CAD3F5";
    p["TEXT_DIM"        ] = "#959AB7";
    p["TEXT_FAINT"      ] = "#6E738D";
    p["NAV_TEXT"        ] = "#A5ADCB";
    p["ACCENT"          ] = "#8BD5CA";
    p["ACCENT_DARK"     ] = "#72afa6";
    p["ACCENT_LIGHT"    ] = "#7DC4E4";
    p["ACCENT_SOFT"     ] = "#90cde8";
    p["ACCENT_TEXT"     ] = "#181926";
    p["TINT1"           ] = "rgba(139,213,202,0.14)";
    p["TINT2"           ] = "rgba(139,213,202,0.24)";
    p["TINT3"           ] = "rgba(139,213,202,0.34)";
    p["TINT_BORDER"     ] = "rgba(139,213,202,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#494E66";
    p["BTN_HOVER"       ] = "#5B6078";
    p["BTN_PRESS"       ] = "#35394c";
    p["BTN_LINE"        ] = "#656a80";
    p["BTN_TEXT"        ] = "#c6cff1";
    p["DISABLED_BG"     ] = "#2d3044";
    p["DISABLED_TEXT"   ] = "#5c6078";
    p["DISABLED_LINE"   ] = "#2d3044";
    p["SCROLL"          ] = "#43485f";
    p["SCROLL_HOVER"    ] = "#64697d";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#A6DA95";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#363A4F";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#1E2030";
    p["ICON"            ] = "#A5ADCB";
    return p;
}

// Catppuccin Frappe: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_frappe()
{
    Palette p = base();
    p["BG"              ] = "#303446";
    p["BG_DEEP"         ] = "#232634";
    p["HEADER_TOP"      ] = "#363a4c";
    p["HEADER_BOT"      ] = "#303446";
    p["SURFACE"         ] = "#414559";
    p["SURFACE_2"       ] = "#383c50";
    p["SURFACE_HI"      ] = "#4C5066";
    p["FIELD"           ] = "#232634";
    p["FIELD_DISABLED"  ] = "#2a2d3d";
    p["LINE"            ] = "#555A70";
    p["LINE_SOFT"       ] = "#3a3e51";
    p["LINE_STRONG"     ] = "#555A70";
    p["LINE_CARD"       ] = "#414559";
    p["TEXT"            ] = "#c4cef2";
    p["TEXT_STRONG"     ] = "#C6D0F5";
    p["TEXT_DIM"        ] = "#9CA0BC";
    p["TEXT_FAINT"      ] = "#737994";
    p["NAV_TEXT"        ] = "#ADB3CE";
    p["ACCENT"          ] = "#81C8BE";
    p["ACCENT_DARK"     ] = "#6aa49c";
    p["ACCENT_LIGHT"    ] = "#85C1DC";
    p["ACCENT_SOFT"     ] = "#97cae1";
    p["ACCENT_TEXT"     ] = "#232634";
    p["TINT1"           ] = "rgba(129,200,190,0.14)";
    p["TINT2"           ] = "rgba(129,200,190,0.24)";
    p["TINT3"           ] = "rgba(129,200,190,0.34)";
    p["TINT_BORDER"     ] = "rgba(129,200,190,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#555A70";
    p["BTN_HOVER"       ] = "#626880";
    p["BTN_PRESS"       ] = "#414558";
    p["BTN_LINE"        ] = "#6b7188";
    p["BTN_TEXT"        ] = "#c4cdf1";
    p["DISABLED_BG"     ] = "#383c50";
    p["DISABLED_TEXT"   ] = "#626880";
    p["DISABLED_LINE"   ] = "#383c50";
    p["SCROLL"          ] = "#4f546a";
    p["SCROLL_HOVER"    ] = "#6e7385";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#A6D189";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#414559";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#292C3C";
    p["ICON"            ] = "#ADB3CE";
    return p;
}

// Catppuccin Latte: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_latte()
{
    Palette p = base();
    p["CHECK_ICON"]    = ":/icons/check_light.svg";
    p["BG"              ] = "#EFF1F5";
    p["BG_DEEP"         ] = "#DCE0E8";
    p["HEADER_TOP"      ] = "#eff1f5";
    p["HEADER_BOT"      ] = "#EFF1F5";
    p["SURFACE"         ] = "#FFFFFF";
    p["SURFACE_2"       ] = "#f7f8fa";
    p["SURFACE_HI"      ] = "#F4F6FA";
    p["FIELD"           ] = "#ffffff";
    p["FIELD_DISABLED"  ] = "#e6e8ee";
    p["LINE"            ] = "#BCC0CC";
    p["LINE_SOFT"       ] = "#d0d4dc";
    p["LINE_STRONG"     ] = "#ACB0BE";
    p["LINE_CARD"       ] = "#d0d3db";
    p["TEXT"            ] = "#4d506a";
    p["TEXT_STRONG"     ] = "#4C4F69";
    p["TEXT_DIM"        ] = "#7C7F93";
    p["TEXT_FAINT"      ] = "#8C8FA0";
    p["NAV_TEXT"        ] = "#5C5F77";
    p["ACCENT"          ] = "#179299";
    p["ACCENT_DARK"     ] = "#13787d";
    p["ACCENT_LIGHT"    ] = "#209FB5";
    p["ACCENT_SOFT"     ] = "#1b879a";
    p["ACCENT_TEXT"     ] = "#FFFFFF";
    p["TINT1"           ] = "rgba(23,146,153,0.14)";
    p["TINT2"           ] = "rgba(23,146,153,0.24)";
    p["TINT3"           ] = "rgba(23,146,153,0.34)";
    p["TINT_BORDER"     ] = "rgba(23,146,153,0.45)";
    p["ON_SELECT"       ] = "#4C4F69";
    p["BTN"             ] = "#DCE0E8";
    p["BTN_HOVER"       ] = "#CCD0DA";
    p["BTN_PRESS"       ] = "#dce0e8";
    p["BTN_LINE"        ] = "#c0c4cd";
    p["BTN_TEXT"        ] = "#4e516a";
    p["DISABLED_BG"     ] = "#f7f8fa";
    p["DISABLED_TEXT"   ] = "#a5a8b5";
    p["DISABLED_LINE"   ] = "#d6d8e0";
    p["SCROLL"          ] = "#b6bac6";
    p["SCROLL_HOVER"    ] = "#9296a2";
    p["DANGER_TINT"     ] = "rgba(176,52,52,0.10)";
    p["DANGER_TINT2"    ] = "rgba(176,52,52,0.20)";
    p["DANGER_LINE"     ] = "#a44a4a";
    p["DANGER_TEXT"     ] = "#8c2020";
    p["DANGER_TEXT_HI"  ] = "#6d1414";
    p["OK"              ] = "#40A02B";
    p["WARN"            ] = "#8a6100";
    p["TOOLTIP_BG"      ] = "#FFFFFF";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#e8e6e1";
    p["SIDEBAR_BG"      ] = "#E6E9EF";
    p["ICON"            ] = "#5C5F77";
    return p;
}

// T3 Chat: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_t3_chat()
{
    Palette p = base();
    p["BG"              ] = "#1F1A24";
    p["BG_DEEP"         ] = "#19141E";
    p["HEADER_TOP"      ] = "#26212b";
    p["HEADER_BOT"      ] = "#1F1A24";
    p["SURFACE"         ] = "#2F2B34";
    p["SURFACE_2"       ] = "#27222c";
    p["SURFACE_HI"      ] = "#37323C";
    p["FIELD"           ] = "#19141E";
    p["FIELD_DISABLED"  ] = "#1c1721";
    p["LINE"            ] = "#3B3740";
    p["LINE_SOFT"       ] = "#302b35";
    p["LINE_STRONG"     ] = "#48444D";
    p["LINE_CARD"       ] = "#37333c";
    p["TEXT"            ] = "#f5f4f7";
    p["TEXT_STRONG"     ] = "#F9F8FB";
    p["TEXT_DIM"        ] = "#A6A4A9";
    p["TEXT_FAINT"      ] = "#817E85";
    p["NAV_TEXT"        ] = "#C9C7CC";
    p["ACCENT"          ] = "#A3004C";
    p["ACCENT_DARK"     ] = "#86003e";
    p["ACCENT_LIGHT"    ] = "#B42E6C";
    p["ACCENT_SOFT"     ] = "#bf4d82";
    p["ACCENT_TEXT"     ] = "#FFFFFF";
    p["TINT1"           ] = "rgba(163,0,76,0.14)";
    p["TINT2"           ] = "rgba(163,0,76,0.24)";
    p["TINT3"           ] = "rgba(163,0,76,0.34)";
    p["TINT_BORDER"     ] = "rgba(163,0,76,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#3E3942";
    p["BTN_HOVER"       ] = "#48444D";
    p["BTN_PRESS"       ] = "#2f2a34";
    p["BTN_LINE"        ] = "#534f58";
    p["BTN_TEXT"        ] = "#f4f3f6";
    p["DISABLED_BG"     ] = "#27222c";
    p["DISABLED_TEXT"   ] = "#68656d";
    p["DISABLED_LINE"   ] = "#2d2832";
    p["SCROLL"          ] = "#423e47";
    p["SCROLL_HOVER"    ] = "#636068";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#76CF8A";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#2F2B34";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#19141E";
    p["ICON"            ] = "#C9C7CC";
    return p;
}

// T3 Chat Light: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_t3_chat_light()
{
    Palette p = base();
    p["CHECK_ICON"]    = ":/icons/check_light.svg";
    p["BG"              ] = "#FDF7FD";
    p["BG_DEEP"         ] = "#F3EDF3";
    p["HEADER_TOP"      ] = "#fdf7fd";
    p["HEADER_BOT"      ] = "#FDF7FD";
    p["SURFACE"         ] = "#FFFFFF";
    p["SURFACE_2"       ] = "#fefbfe";
    p["SURFACE_HI"      ] = "#FEFBFE";
    p["FIELD"           ] = "#ffffff";
    p["FIELD_DISABLED"  ] = "#f8f2f8";
    p["LINE"            ] = "#E1D3E2";
    p["LINE_SOFT"       ] = "#ece1ed";
    p["LINE_STRONG"     ] = "#D0BDD1";
    p["LINE_CARD"       ] = "#eae0eb";
    p["TEXT"            ] = "#531c57";
    p["TEXT_STRONG"     ] = "#501854";
    p["TEXT_DIM"        ] = "#926D94";
    p["TEXT_FAINT"      ] = "#AF93B1";
    p["NAV_TEXT"        ] = "#764979";
    p["ACCENT"          ] = "#DB2777";
    p["ACCENT_DARK"     ] = "#b42062";
    p["ACCENT_LIGHT"    ] = "#B42062";
    p["ACCENT_SOFT"     ] = "#991b53";
    p["ACCENT_TEXT"     ] = "#FFFFFF";
    p["TINT1"           ] = "rgba(219,39,119,0.14)";
    p["TINT2"           ] = "rgba(219,39,119,0.24)";
    p["TINT3"           ] = "rgba(219,39,119,0.34)";
    p["TINT_BORDER"     ] = "rgba(219,39,119,0.45)";
    p["ON_SELECT"       ] = "#501854";
    p["BTN"             ] = "#F1E7F1";
    p["BTN_HOVER"       ] = "#E8DCE9";
    p["BTN_PRESS"       ] = "#f2e9f2";
    p["BTN_LINE"        ] = "#dacfdb";
    p["BTN_TEXT"        ] = "#541d58";
    p["DISABLED_BG"     ] = "#fefbfe";
    p["DISABLED_TEXT"   ] = "#c2acc4";
    p["DISABLED_LINE"   ] = "#efe5f0";
    p["SCROLL"          ] = "#d7c6d8";
    p["SCROLL_HOVER"    ] = "#b1a1b2";
    p["DANGER_TINT"     ] = "rgba(176,52,52,0.10)";
    p["DANGER_TINT2"    ] = "rgba(176,52,52,0.20)";
    p["DANGER_LINE"     ] = "#a44a4a";
    p["DANGER_TEXT"     ] = "#8c2020";
    p["DANGER_TEXT_HI"  ] = "#6d1414";
    p["OK"              ] = "#298646";
    p["WARN"            ] = "#8a6100";
    p["TOOLTIP_BG"      ] = "#FFFFFF";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#e8e6e1";
    p["SIDEBAR_BG"      ] = "#F3EDF3";
    p["ICON"            ] = "#764979";
    return p;
}

// T3 Grove: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_grove()
{
    Palette p = base();
    p["BG"              ] = "#1B2821";
    p["BG_DEEP"         ] = "#15221B";
    p["HEADER_TOP"      ] = "#222e28";
    p["HEADER_BOT"      ] = "#1B2821";
    p["SURFACE"         ] = "#2C3832";
    p["SURFACE_2"       ] = "#24302a";
    p["SURFACE_HI"      ] = "#343F39";
    p["FIELD"           ] = "#15221B";
    p["FIELD_DISABLED"  ] = "#18251e";
    p["LINE"            ] = "#39433E";
    p["LINE_SOFT"       ] = "#2d3832";
    p["LINE_STRONG"     ] = "#46504B";
    p["LINE_CARD"       ] = "#35403a";
    p["TEXT"            ] = "#fbf6fb";
    p["TEXT_STRONG"     ] = "#FFFAFF";
    p["TEXT_DIM"        ] = "#A8AAAB";
    p["TEXT_FAINT"      ] = "#828685";
    p["NAV_TEXT"        ] = "#CDCCCE";
    p["ACCENT"          ] = "#69D69A";
    p["ACCENT_DARK"     ] = "#56af7e";
    p["ACCENT_LIGHT"    ] = "#84DDAC";
    p["ACCENT_SOFT"     ] = "#96e2b8";
    p["ACCENT_TEXT"     ] = "#0A0C0E";
    p["TINT1"           ] = "rgba(105,214,154,0.14)";
    p["TINT2"           ] = "rgba(105,214,154,0.24)";
    p["TINT3"           ] = "rgba(105,214,154,0.34)";
    p["TINT_BORDER"     ] = "rgba(105,214,154,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#3B4540";
    p["BTN_HOVER"       ] = "#46504B";
    p["BTN_PRESS"       ] = "#2c3731";
    p["BTN_LINE"        ] = "#515a56";
    p["BTN_TEXT"        ] = "#faf5fa";
    p["DISABLED_BG"     ] = "#24302a";
    p["DISABLED_TEXT"   ] = "#686e6c";
    p["DISABLED_LINE"   ] = "#2a3630";
    p["SCROLL"          ] = "#404a45";
    p["SCROLL_HOVER"    ] = "#626a66";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#76CF8A";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#2C3832";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#15221B";
    p["ICON"            ] = "#CDCCCE";
    return p;
}

// T3 Ocean: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_ocean()
{
    Palette p = base();
    p["BG"              ] = "#17212B";
    p["BG_DEEP"         ] = "#111B25";
    p["HEADER_TOP"      ] = "#1e2831";
    p["HEADER_BOT"      ] = "#17212B";
    p["SURFACE"         ] = "#28313B";
    p["SURFACE_2"       ] = "#202933";
    p["SURFACE_HI"      ] = "#313942";
    p["FIELD"           ] = "#111B25";
    p["FIELD_DISABLED"  ] = "#141e28";
    p["LINE"            ] = "#353D47";
    p["LINE_SOFT"       ] = "#29323c";
    p["LINE_STRONG"     ] = "#434A53";
    p["LINE_CARD"       ] = "#313943";
    p["TEXT"            ] = "#fbf6fb";
    p["TEXT_STRONG"     ] = "#FFFAFF";
    p["TEXT_DIM"        ] = "#A7A8AE";
    p["TEXT_FAINT"      ] = "#7F838A";
    p["NAV_TEXT"        ] = "#CCCAD0";
    p["ACCENT"          ] = "#70B9EE";
    p["ACCENT_DARK"     ] = "#5c98c3";
    p["ACCENT_LIGHT"    ] = "#8AC6F1";
    p["ACCENT_SOFT"     ] = "#9ccff3";
    p["ACCENT_TEXT"     ] = "#0A0C0E";
    p["TINT1"           ] = "rgba(112,185,238,0.14)";
    p["TINT2"           ] = "rgba(112,185,238,0.24)";
    p["TINT3"           ] = "rgba(112,185,238,0.34)";
    p["TINT_BORDER"     ] = "rgba(112,185,238,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#373F49";
    p["BTN_HOVER"       ] = "#434A53";
    p["BTN_PRESS"       ] = "#28313b";
    p["BTN_LINE"        ] = "#4e555d";
    p["BTN_TEXT"        ] = "#faf5fa";
    p["DISABLED_BG"     ] = "#202933";
    p["DISABLED_TEXT"   ] = "#656a72";
    p["DISABLED_LINE"   ] = "#262f39";
    p["SCROLL"          ] = "#3c444d";
    p["SCROLL_HOVER"    ] = "#5f656d";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#76CF8A";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#28313B";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#111B25";
    p["ICON"            ] = "#CCCAD0";
    return p;
}

// T3 Ember: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_ember()
{
    Palette p = base();
    p["BG"              ] = "#291E1A";
    p["BG_DEEP"         ] = "#231814";
    p["HEADER_TOP"      ] = "#2f2521";
    p["HEADER_BOT"      ] = "#291E1A";
    p["SURFACE"         ] = "#392E2B";
    p["SURFACE_2"       ] = "#312622";
    p["SURFACE_HI"      ] = "#413633";
    p["FIELD"           ] = "#231814";
    p["FIELD_DISABLED"  ] = "#261b17";
    p["LINE"            ] = "#453B38";
    p["LINE_SOFT"       ] = "#3a2f2c";
    p["LINE_STRONG"     ] = "#524846";
    p["LINE_CARD"       ] = "#413734";
    p["TEXT"            ] = "#fbf6fb";
    p["TEXT_STRONG"     ] = "#FFFAFF";
    p["TEXT_DIM"        ] = "#AEA6A8";
    p["TEXT_FAINT"      ] = "#898181";
    p["NAV_TEXT"        ] = "#D0CACD";
    p["ACCENT"          ] = "#F09A64";
    p["ACCENT_DARK"     ] = "#c57e52";
    p["ACCENT_LIGHT"    ] = "#F3AC80";
    p["ACCENT_SOFT"     ] = "#f5b893";
    p["ACCENT_TEXT"     ] = "#0A0C0E";
    p["TINT1"           ] = "rgba(240,154,100,0.14)";
    p["TINT2"           ] = "rgba(240,154,100,0.24)";
    p["TINT3"           ] = "rgba(240,154,100,0.34)";
    p["TINT_BORDER"     ] = "rgba(240,154,100,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#473D3A";
    p["BTN_HOVER"       ] = "#524846";
    p["BTN_PRESS"       ] = "#392e2b";
    p["BTN_LINE"        ] = "#5c5351";
    p["BTN_TEXT"        ] = "#faf5fa";
    p["DISABLED_BG"     ] = "#312622";
    p["DISABLED_TEXT"   ] = "#716867";
    p["DISABLED_LINE"   ] = "#372c29";
    p["SCROLL"          ] = "#4c423f";
    p["SCROLL_HOVER"    ] = "#6c6362";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#76CF8A";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#392E2B";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#231814";
    p["ICON"            ] = "#D0CACD";
    return p;
}

// T3 Iris: ported from the WinForms Quality of Life Mod Manager 1.x palette.
Palette theme_iris()
{
    Palette p = base();
    p["BG"              ] = "#1D1929";
    p["BG_DEEP"         ] = "#171323";
    p["HEADER_TOP"      ] = "#24202f";
    p["HEADER_BOT"      ] = "#1D1929";
    p["SURFACE"         ] = "#2E2A39";
    p["SURFACE_2"       ] = "#262231";
    p["SURFACE_HI"      ] = "#363241";
    p["FIELD"           ] = "#171323";
    p["FIELD_DISABLED"  ] = "#1a1626";
    p["LINE"            ] = "#3A3645";
    p["LINE_SOFT"       ] = "#2e2a3a";
    p["LINE_STRONG"     ] = "#484452";
    p["LINE_CARD"       ] = "#363241";
    p["TEXT"            ] = "#fbf6fb";
    p["TEXT_STRONG"     ] = "#FFFAFF";
    p["TEXT_DIM"        ] = "#A9A4AE";
    p["TEXT_FAINT"      ] = "#837E89";
    p["NAV_TEXT"        ] = "#CDC8D0";
    p["ACCENT"          ] = "#9D7DF2";
    p["ACCENT_DARK"     ] = "#8166c6";
    p["ACCENT_LIGHT"    ] = "#AF94F4";
    p["ACCENT_SOFT"     ] = "#bba4f6";
    p["ACCENT_TEXT"     ] = "#0A0C0E";
    p["TINT1"           ] = "rgba(157,125,242,0.14)";
    p["TINT2"           ] = "rgba(157,125,242,0.24)";
    p["TINT3"           ] = "rgba(157,125,242,0.34)";
    p["TINT_BORDER"     ] = "rgba(157,125,242,0.45)";
    p["ON_SELECT"       ] = "#ffffff";
    p["BTN"             ] = "#3D3847";
    p["BTN_HOVER"       ] = "#484452";
    p["BTN_PRESS"       ] = "#2e2939";
    p["BTN_LINE"        ] = "#534f5c";
    p["BTN_TEXT"        ] = "#faf5fa";
    p["DISABLED_BG"     ] = "#262231";
    p["DISABLED_TEXT"   ] = "#6a6571";
    p["DISABLED_LINE"   ] = "#2c2837";
    p["SCROLL"          ] = "#423e4c";
    p["SCROLL_HOVER"    ] = "#63606c";
    p["DANGER_TINT"     ] = "rgba(198,106,112,0.10)";
    p["DANGER_TINT2"    ] = "rgba(198,106,112,0.22)";
    p["DANGER_LINE"     ] = "#7d454b";
    p["DANGER_TEXT"     ] = "#e2a3a8";
    p["DANGER_TEXT_HI"  ] = "#f3c3c6";
    p["OK"              ] = "#76CF8A";
    p["WARN"            ] = "#f0c36d";
    p["TOOLTIP_BG"      ] = "#2E2A39";
    p["HERO_TEXT"       ] = "#ffffff";
    p["HERO_SUB"        ] = "#c5c6da";
    p["SIDEBAR_BG"      ] = "#171323";
    p["ICON"            ] = "#CDC8D0";
    return p;
}

struct ThemeDef { const char *key; const char *name; Palette (*build)(); };
const ThemeDef kThemes[] = {
    {"nocturne",      "Nocturne",              nocturne},
    {"classic_dark",  "Classic Things dark",   classicDark},
    {"classic_light", "Classic Things light",  classicLight},
    {"qol_classic",   "Classic Dark",          theme_qol_classic},
    {"oled",          "OLED Black",            theme_oled},
    {"mocha",         "Catppuccin Mocha",      theme_mocha},
    {"macchiato",     "Catppuccin Macchiato",  theme_macchiato},
    {"frappe",        "Catppuccin Frappe",     theme_frappe},
    {"latte",         "Catppuccin Latte",      theme_latte},
    {"t3_chat",       "T3 Chat",               theme_t3_chat},
    {"t3_chat_light", "T3 Chat Light",         theme_t3_chat_light},
    {"grove",         "T3 Grove",              theme_grove},
    {"ocean",         "T3 Ocean",              theme_ocean},
    {"ember",         "T3 Ember",              theme_ember},
    {"iris",          "T3 Iris",               theme_iris},
};

Palette paletteFor(const QString &key)
{
    for (const ThemeDef &t : kThemes)
        if (key == QLatin1String(t.key))
            return t.build();
    return nocturne();
}

QString g_current = QStringLiteral("nocturne");
Palette g_palette = nocturne();

} // namespace

ThemeHub &ThemeHub::instance()
{
    static ThemeHub hub;
    return hub;
}

namespace Theme {

QString token(const QString &name) { return g_palette.value(name); }
QColor color(const QString &name) { return QColor(token(name)); }
int radius() { return token(QStringLiteral("R")).startsWith(QLatin1Char('0')) ? 0 : 8; }
bool isSquare() { return radius() == 0; }

QIcon icon(const QString &resourcePath)
{
    QPixmap src = QIcon(resourcePath).pixmap(QSize(64, 64));
    if (src.isNull())
        return QIcon(resourcePath);
    QPixmap out(src.size());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawPixmap(0, 0, src);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color(QStringLiteral("ICON")));
    p.end();
    return QIcon(out);
}

QString accent()     { return token(QStringLiteral("ACCENT")); }
QString accentDark() { return token(QStringLiteral("ACCENT_DARK")); }
QString accentText() { return token(QStringLiteral("ACCENT_TEXT")); }
QString muted()      { return token(QStringLiteral("TEXT_DIM")); }
QString line()       { return token(QStringLiteral("LINE")); }

QStringList names()
{
    QStringList out;
    for (const ThemeDef &t : kThemes)
        out << QString::fromLatin1(t.key);
    return out;
}

QString displayName(const QString &themeKey)
{
    const QString key = normalizeKey(themeKey);
    for (const ThemeDef &t : kThemes)
        if (key == QLatin1String(t.key))
            return QString::fromLatin1(t.name);
    return QStringLiteral("Nocturne");
}

QString normalizeKey(const QString &themeKey)
{
    QString k = themeKey.trimmed().toLower();
    k.replace(QLatin1Char('-'), QLatin1Char('_'));
    if (k == QLatin1String("classicdark") || k == QLatin1String("classic things") || k == QLatin1String("classic_things"))
        return QStringLiteral("classic_dark");
    if (k == QLatin1String("classiclight"))
        return QStringLiteral("classic_light");
    // The 1.x app called its default "classic"; that is our qol_classic.
    if (k == QLatin1String("classic"))
        return QStringLiteral("qol_classic");
    for (const ThemeDef &t : kThemes)
        if (k == QLatin1String(t.key))
            return k;
    return QStringLiteral("nocturne"); // includes the old "DarkAmber"
}

QString current() { return g_current; }

void apply(const QString &themeKey)
{
    const QString key = themeKey.trimmed().isEmpty() ? g_current : normalizeKey(themeKey);
    g_current = key;
    g_palette = paletteFor(key);

    QFile file(QStringLiteral(":/style/base.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QString qss = QTextStream(&file).readAll();

    // Tokens mais longos primeiro: @ACCENT_DARK@ antes de @ACCENT@.
    QStringList keys = g_palette.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });
    for (const QString &name : keys)
        qss.replace(QLatin1Char('@') + name + QLatin1Char('@'), g_palette.value(name));

    if (auto *app = qApp)
        app->setStyleSheet(qss);

    emit ThemeHub::instance().themeChanged();
}

} // namespace Theme
