#pragma once

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>
#include <QStyleFactory>

#include "core/Block.hpp"

namespace Theme {

enum class Variant {
    Deep,
    Wine
};

struct Colors {
    QString name;
    QColor background;
    QColor surface;
    QColor text;
    QColor accent;
    QColor aqua;
};

struct Alphas {
    double title = 0.80;
    double subtitle = 0.65;
    double body = 0.55;
    double muted = 0.45;
    double frame = 0.22;
    double wizardFrame = 0.70;
    double wizardPlaceholder = 0.80;
    double selection = 0.45;
    double scrollHandle = 0.22;
    double scrollHandleHover = 0.38;
    double placeholder = 0.40;
};

struct Fonts {
    QString family = "Segoe UI";
    QString mono = "Consolas";
};

struct Metrics {
    int windowWidth = 800;
    int windowHeight = 600;
    int blockSpacing = 16;
    int sideMargin = 20;
    int cardPadding = 8;
    int cardBorderWidth = 0;
    int imageRadius = 8;
    int cardChrome() const { return 2 * (cardPadding + cardBorderWidth); }
};

inline QColor withAlpha (const QColor& c, double alpha) {
    QColor out = c;
    out.setAlphaF (alpha);
    return out;
}

inline QColor mix (const QColor& over, const QColor& under, double amount) {
    return QColor::fromRgbF (
        over.redF() * amount + under.redF() * (1.0 - amount),
        over.greenF() * amount + under.greenF() * (1.0 - amount),
        over.blueF() * amount + under.blueF() * (1.0 - amount)
    );
}

inline QString css (const QColor& c, double alpha = 1.0) {
    return QString ("rgba(%1, %2, %3, %4)")
        .arg (c.red())
        .arg (c.green())
        .arg (c.blue())
        .arg (alpha, 0, 'f', 2);
}

inline Colors makeColors (Variant variant) {
    Colors c;
    c.text = QColor ("#CEC3C1");
    c.accent = QColor ("#DE3C4B");
    c.aqua = QColor ("#87F5FB");
    if (variant == Variant::Deep) {
        c.name = "Deep";
        c.background = QColor ("#14000B");
        c.surface = QColor ("#240115");
    } else {
        c.name = "Wine";
        c.background = QColor ("#240115");
        c.surface = QColor ("#2F131E");
    }
    return c;
}

inline Variant& variantSlot() {
    static Variant v = Variant::Deep;
    return v;
}

inline Colors& colorsSlot() {
    static Colors c = makeColors (Variant::Deep);
    return c;
}

inline const Colors& colors() { return colorsSlot(); }

inline const Alphas& alphas() {
    static const Alphas a;
    return a;
}

inline const Fonts& fonts() {
    static const Fonts f;
    return f;
}

inline const Metrics& metrics() {
    static const Metrics m;
    return m;
}

inline void setVariant (Variant variant) {
    variantSlot() = variant;
    colorsSlot() = makeColors (variant);
}

inline void cycleVariant() {
    setVariant (variantSlot() == Variant::Deep ? Variant::Wine : Variant::Deep);
}

inline QFont editorFont (BlockType type) {
    const Fonts& f = fonts();
    switch (type) {
        case BlockType::Title:
            return QFont (f.family, 16);
        case BlockType::Subtitle:
            return QFont (f.family, 14);
        case BlockType::Image: {
            QFont font (f.family, 11);
            font.setItalic (true);
            return font;
        }
        case BlockType::Equation:
            return QFont (f.mono, 12);
        default:
            return QFont (f.family, 13);
    }
}

inline QFont labelFont (BlockType type) {
    const Fonts& f = fonts();
    switch (type) {
        case BlockType::Title:
            return QFont (f.family, 15);
        case BlockType::Subtitle:
            return QFont (f.family, 13);
        default:
            return QFont (f.family, 11, QFont::Normal);
    }
}

inline QString labelStyle (BlockType type) {
    const Colors& c = colors();
    const Alphas& a = alphas();
    const double alpha = type == BlockType::Title ? a.title
                       : type == BlockType::Subtitle ? a.subtitle
                       : a.body;
    return QString ("QLabel { color: %1; padding: 6px; background: transparent; }").arg (css (c.text, alpha));
}

inline QString cardStyle() {
    return QString ("QLabel { padding: %1px; background: transparent; border: none; }").arg (metrics().cardPadding);
}

inline QString mutedLabelStyle() {
    return QString ("QLabel { color: %1; font-style: italic; padding: 6px; background: transparent; }")
        .arg (css (colors().text, alphas().muted));
}

inline QString errorLabelStyle() {
    return QString ("QLabel { color: %1; font-weight: bold; padding: 6px; background: transparent; }")
        .arg (css (colors().accent));
}

inline QString statusStyle() {
    return QString ("color: %1; font-style: italic;").arg (css (colors().text, alphas().muted));
}

inline QString editorStyle() {
    const Colors& c = colors();
    const Alphas& a = alphas();
    return QString ("QTextEdit { background: transparent; color: %1; border: 1px solid %2; padding: 6px; border-radius: 2px;"
                    " selection-background-color: %3; selection-color: %1; }")
        .arg (css (c.text))
        .arg (css (c.text, a.frame))
        .arg (css (c.accent, a.selection));
}

inline QString wizardStyle() {
    const Colors& c = colors();
    const Alphas& a = alphas();
    return QString ("QTextEdit { background: transparent; color: %1; border: 1px solid %2; padding: 6px; border-radius: 2px;"
                    " selection-background-color: %3; selection-color: %1; placeholder-text-color: %4; }")
        .arg (css (c.text))
        .arg (css (c.aqua, a.wizardFrame))
        .arg (css (c.accent, a.selection))
        .arg (css (c.aqua, a.wizardPlaceholder));
}

inline QString dialogStyle() {
    const Colors& c = colors();
    const Alphas& a = alphas();
    const Fonts& f = fonts();
    return QString ("QDialog { background-color: %1; }"
                    "QLabel { color: %2; font-family: '%3'; }"
                    "QPushButton { background: transparent; color: %2; border: 1px solid %4; border-radius: 2px;"
                    " padding: 6px 12px; font-family: '%3'; }"
                    "QPushButton:hover { border-color: %5; }"
                    "QLineEdit, QComboBox, QAbstractItemView { background-color: %6; color: %2;"
                    " selection-background-color: %7; selection-color: %2; }"
                    "QLineEdit, QComboBox { border: 1px solid %4; border-radius: 2px; padding: 3px; }")
        .arg (css (c.surface))
        .arg (css (c.text))
        .arg (f.family)
        .arg (css (c.text, 0.35))
        .arg (css (c.accent))
        .arg (css (c.background))
        .arg (css (c.accent, a.selection));
}

inline QString appStyle() {
    const Colors& c = colors();
    const Alphas& a = alphas();
    return QString ("QScrollArea { background: transparent; border: none; }"
                    "QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }"
                    "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; margin: 2px; }"
                    "QScrollBar::handle:vertical:hover { background: %2; }"
                    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
                    "QToolTip { background-color: %3; color: %4; border: 1px solid %5; padding: 4px; }")
        .arg (css (c.text, a.scrollHandle))
        .arg (css (c.text, a.scrollHandleHover))
        .arg (css (c.surface))
        .arg (css (c.text))
        .arg (css (c.text, 0.3));
}

inline QPalette makePalette() {
    const Colors& c = colors();
    const Alphas& a = alphas();
    QPalette p;
    p.setColor (QPalette::Window, c.background);
    p.setColor (QPalette::WindowText, c.text);
    p.setColor (QPalette::Base, c.background);
    p.setColor (QPalette::AlternateBase, c.surface);
    p.setColor (QPalette::Text, c.text);
    p.setColor (QPalette::Button, c.surface);
    p.setColor (QPalette::ButtonText, c.text);
    p.setColor (QPalette::BrightText, c.accent);
    p.setColor (QPalette::Highlight, mix (c.accent, c.background, a.selection + 0.1));
    p.setColor (QPalette::HighlightedText, c.text);
    p.setColor (QPalette::PlaceholderText, withAlpha (c.text, a.placeholder));
    p.setColor (QPalette::ToolTipBase, c.surface);
    p.setColor (QPalette::ToolTipText, c.text);
    p.setColor (QPalette::Link, c.aqua);
    p.setColor (QPalette::Disabled, QPalette::Text, withAlpha (c.text, 0.35));
    p.setColor (QPalette::Disabled, QPalette::WindowText, withAlpha (c.text, 0.35));
    p.setColor (QPalette::Disabled, QPalette::ButtonText, withAlpha (c.text, 0.35));
    return p;
}

inline void applyTo (QApplication& app) {
    app.setStyle (QStyleFactory::create ("Fusion"));
    app.setPalette (makePalette());
    app.setStyleSheet (appStyle());
}

}
