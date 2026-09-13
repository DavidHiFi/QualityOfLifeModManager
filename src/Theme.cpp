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

Palette paletteFor(const QString &key)
{
    if (key == QLatin1String("classic_dark"))
        return classicDark();
    if (key == QLatin1String("classic_light"))
        return classicLight();
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
    return {QStringLiteral("nocturne"),
            QStringLiteral("classic_dark"),
            QStringLiteral("classic_light")};
}

QString displayName(const QString &themeKey)
{
    const QString key = normalizeKey(themeKey);
    if (key == QLatin1String("classic_dark"))
        return QCoreApplication::translate("Theme", "Classic Things · Escuro");
    if (key == QLatin1String("classic_light"))
        return QCoreApplication::translate("Theme", "Classic Things · Claro");
    return QCoreApplication::translate("Theme", "Nocturne (padrão)");
}

QString normalizeKey(const QString &themeKey)
{
    const QString k = themeKey.trimmed().toLower();
    if (k == QLatin1String("classic_dark") || k == QLatin1String("classicdark")
        || k == QLatin1String("classic things") || k == QLatin1String("classic_things"))
        return QStringLiteral("classic_dark");
    if (k == QLatin1String("classic_light") || k == QLatin1String("classiclight"))
        return QStringLiteral("classic_light");
    return QStringLiteral("nocturne"); // inclui o antigo "DarkAmber"
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
