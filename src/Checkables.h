#pragma once
#include "Theme.h"
#include <QCheckBox>
#include <QAbstractItemView>
#include <QComboBox>
#include <QRadioButton>
#include <QEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

// Hand-drawn checkbox and radio.
//
// Qt often fails to recompute the label width after QSS resizes the indicator,
// so text gets clipped ("Multiplaye"). This widget paints the mark and the
// label itself and reports its own sizeHint. No Q_OBJECT — no moc needed.
namespace Ui {

// Combo that ignores wheel unless the popup is open (so page scroll does not change the value).
class ComboBox : public QComboBox
{
public:
    using QComboBox::QComboBox;
protected:
    void wheelEvent(QWheelEvent *e) override
    {
        if (view() && view()->isVisible())
            QComboBox::wheelEvent(e);
        else
            e->ignore();
    }
};

namespace detail {
// Todas as cores vem do tema atual (Nocturne / Classic Things).
inline QColor kText()        { return Theme::color("TEXT"); }
inline QColor kTextOn()      { return Theme::color("TEXT_STRONG"); }
inline QColor kTextOff()     { return Theme::color("DISABLED_TEXT"); }
inline QColor kFill()        { return Theme::color("FIELD"); }
inline QColor kBorder()      { return Theme::color("LINE_STRONG"); }
inline QColor kAccent()      { return Theme::color("ACCENT"); }
inline QColor kAccentLight() { return Theme::color("ACCENT_LIGHT"); }
inline QColor kAccentDark()  { return Theme::color("ACCENT_DARK"); }
inline QColor kHoverFill()   { return Theme::color("SURFACE_HI"); }
inline QColor kOffFill()     { return Theme::color("DISABLED_BG"); }
inline QColor kOffLine()     { return Theme::color("DISABLED_LINE"); }
inline QColor kInk()         { return Theme::color("ACCENT_TEXT"); }
inline int    kR()           { return Theme::radius() ? 5 : 0; }
// Widgets sobre a arte do jogo usam texto claro em qualquer tema:
// basta setProperty("onArt", true).
inline QColor kArtText()     { return Theme::color("HERO_SUB"); }
inline QColor kArtTextOn()   { return Theme::color("HERO_TEXT"); }

constexpr int kBox = 18;   // lado do indicador
constexpr int kGap = 10;   // espaco entre indicador e texto
// The focus ring is drawn 3px outside the indicator. The indicator used to
// start at x=1, which put the ring at x=-2: clicking a checkbox drew a
// rounded outline whose left side was cut off by the widget edge. The box
// starts far enough in for the ring to fit, and sizeHint pays for it.
constexpr int kFocusPad = 3;
} // namespace detail

class CheckBox : public QCheckBox
{
public:
    explicit CheckBox(const QString &text, QWidget *parent = nullptr)
        : QCheckBox(text, parent)
    {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        const int w = detail::kFocusPad + detail::kBox + detail::kGap
                    + fm.horizontalAdvance(text()) + 4;
        const int h = qMax(detail::kBox + 8, fm.height() + 8);
        return QSize(w, h);
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    bool event(QEvent *e) override
    {
        switch (e->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::Enter:
        case QEvent::Leave:
            update();
            break;
        default:
            break;
        }
        return QCheckBox::event(e);
    }

    void paintEvent(QPaintEvent *) override
    {
        using namespace detail;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const bool on = isChecked();
        const bool hov = underMouse() && isEnabled();
        const bool off = !isEnabled();

        QRectF box(kFocusPad + 1.0, (height() - kBox) / 2.0, kBox - 1.0, kBox - 1.0);

        const int rad = kR();
        p.setRenderHint(QPainter::Antialiasing, rad > 0);

        QColor border = on ? kAccent() : kBorder();
        QColor fill = on ? kAccent() : kFill();
        if (hov) {
            border = kAccent();
            fill = on ? kAccentLight() : kHoverFill();
        }
        if (off) {
            border = kOffLine();
            fill = on ? kOffLine() : kOffFill();
        }

        if (hasFocus()) {
            p.setPen(QPen(kAccent(), 1.0));
            p.setBrush(Qt::NoBrush);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.drawRoundedRect(box.adjusted(-kFocusPad, -kFocusPad, kFocusPad, kFocusPad),
                              rad ? 7 : 0, rad ? 7 : 0);
            p.setRenderHint(QPainter::Antialiasing, rad > 0);
        }

        p.setPen(QPen(border, 2.0));
        p.setBrush(fill);
        p.drawRoundedRect(box, rad, rad);

        if (on) {
            // checkmark
            QPainterPath tick;
            tick.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.53);
            tick.lineTo(box.left() + box.width() * 0.44, box.top() + box.height() * 0.73);
            tick.lineTo(box.left() + box.width() * 0.78, box.top() + box.height() * 0.29);
            p.setBrush(Qt::NoBrush);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(off ? kTextOff() : kInk(), 2.4,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(tick);
        }

        const int textLeft = int(box.right()) + kGap;
        const QRect textRect(textLeft, 0, width() - textLeft, height());
        const bool art = property("onArt").toBool();
        p.setPen(off ? kTextOff()
                     : (on || hov ? (art ? kArtTextOn() : kTextOn())
                                  : (art ? kArtText() : kText())));
        p.setFont(font());
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
};

class RadioButton : public QRadioButton
{
public:
    explicit RadioButton(const QString &text, QWidget *parent = nullptr)
        : QRadioButton(text, parent)
    {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        setContentsMargins(6, 2, 4, 2);
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        const int w = 8 + detail::kBox + detail::kGap + fm.horizontalAdvance(text()) + 8;
        const int h = qMax(detail::kBox + 14, fm.height() + 12);
        return QSize(w, h);
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    bool event(QEvent *e) override
    {
        switch (e->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::Enter:
        case QEvent::Leave:
            update();
            break;
        default:
            break;
        }
        return QRadioButton::event(e);
    }

    void paintEvent(QPaintEvent *) override
    {
        using namespace detail;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const bool on = isChecked();
        const bool hov = underMouse() && isEnabled();
        const bool off = !isEnabled();

        const bool square = Theme::isSquare();
        p.setRenderHint(QPainter::Antialiasing, !square);

        QRectF circle(5.0, (height() - kBox) / 2.0, kBox, kBox);
        QColor border = (on || hov) ? kAccent() : kBorder();
        if (off)
            border = kOffLine();

        if (hasFocus()) {
            p.setPen(QPen(kAccent(), 1.0));
            p.setBrush(Qt::NoBrush);
            if (square)
                p.drawRect(circle.adjusted(-3, -3, 3, 3));
            else
                p.drawEllipse(circle.adjusted(-3, -3, 3, 3));
        }

        p.setPen(QPen(border, 2.0));
        p.setBrush(off ? kOffFill() : (hov && !on ? kHoverFill() : kFill()));
        if (square)
            p.drawRect(circle);
        else
            p.drawEllipse(circle);

        if (on) {
            p.setPen(Qt::NoPen);
            p.setBrush(off ? kOffLine() : kAccent());
            if (square)
                p.drawRect(circle.adjusted(4, 4, -4, -4));
            else
                p.drawEllipse(circle.adjusted(4, 4, -4, -4));
        }

        const QRect textRect(kBox + kGap, 0, width() - kBox - kGap, height());
        const bool art = property("onArt").toBool();
        p.setPen(off ? kTextOff()
                     : (on || hov ? (art ? kArtTextOn() : kTextOn())
                                  : (art ? kArtText() : kText())));
        p.setFont(font());
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
};

} // namespace Ui
