#pragma once

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QString>
#include <QStyleFactory>

#include "core/Block.hpp"

namespace Theme {

struct Colors {
    QColor background { "#0E0008" };
    QColor surface { "#1A000F" };
    QColor text { "#CEC3C1" };
    QColor accent { "#DE3C4B" };
    QColor aqua { "#87F5FB" };
};

struct Alphas {
    double title = 0.80;
    double subtitle = 0.65;
    double body = 0.55;
    double muted = 0.45;
    double legend = 0.55;
    double wizardLegend = 0.90;
    double frame = 0.24;
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
    QString label = "Segoe UI";
};

struct Sizes {
    double title = 20.0;
    double subtitle = 16.0;
    double body = 13.0;
    double mono = 11.0;
    double label = 10.0;
    double focusScale = 1.3;
    double contentScale = 1.15;
};

struct Metrics {
    int windowWidth = 800;
    int windowHeight = 600;
    int blockSpacing = 10;
    int sideMargin = 20;
    double pageMarginInches = 2.0;
    int maxContentWidth = 0;
    int legendZone = 10;
    int cardPadding = 8;
    int cardBorderWidth = 0;
    int imageRadius = 8;
    int editBorderWidth = 2;
    int caretWidth = 2;
    int transitionMs = 260;
    int fadeHeight = 110;
    int cardChrome() const { return 2 * (cardPadding + cardBorderWidth); }
};

inline QColor withAlpha (const QColor& c, double alpha) {
    QColor out = c;
    out.setAlphaF (alpha);
    return out;
}

inline QColor mix (const QColor& over, const QColor& under, double amount) {
    return QColor::fromRgbF (over.redF() * amount + under.redF() * (1.0 - amount),
                             over.greenF() * amount + under.greenF() * (1.0 - amount),
                             over.blueF() * amount + under.blueF() * (1.0 - amount));
}

inline QString css (const QColor& c, double alpha = 1.0) {
    return QString ("rgba(%1, %2, %3, %4)")
        .arg (c.red())
        .arg (c.green())
        .arg (c.blue())
        .arg (alpha, 0, 'f', 2);
}

inline const Colors& colors() {
    static const Colors c;
    return c;
}

inline const Alphas& alphas() {
    static const Alphas a;
    return a;
}

inline const Sizes& sizes() {
    static const Sizes s;
    return s;
}

inline const Metrics& metrics() {
    static const Metrics m;
    return m;
}

inline Fonts& fontsSlot() {
    static Fonts f;
    return f;
}

inline const Fonts& fonts() { return fontsSlot(); }

inline void setFontFamilies (const QString& family, const QString& mono = QString()) {
    fontsSlot().family = family;
    if (!mono.isEmpty())
        fontsSlot().mono = mono;
}

inline QFont makeFont (const QString& family, double pointSize, bool italic = false) {
    QFont font (family);
    font.setPointSizeF (pointSize);
    font.setItalic (italic);
    return font;
}

inline QFont labelSmallFont() {
    QFont font = makeFont (fonts().label, sizes().label);
    font.setLetterSpacing (QFont::AbsoluteSpacing, 0.8);
    return font;
}

inline QFont labelFont (BlockType type) {
    const Fonts& f = fonts();
    const Sizes& s = sizes();
    switch (type) {
        case BlockType::Title:
            return makeFont (f.family, s.title);
        case BlockType::Subtitle:
            return makeFont (f.family, s.subtitle);
        default:
            return makeFont (f.family, s.body);
    }
}

inline QFont editorFont (BlockType type) {
    const Fonts& f = fonts();
    const Sizes& s = sizes();
    switch (type) {
        case BlockType::Title:
            return makeFont (f.family, s.title * s.focusScale);
        case BlockType::Subtitle:
            return makeFont (f.family, s.subtitle * s.focusScale);
        case BlockType::Image:
            return makeFont (f.family, s.body * s.focusScale, true);
        case BlockType::Equation:
            return makeFont (f.mono, s.mono * s.focusScale);
        default:
            return makeFont (f.family, s.body * s.focusScale);
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
    return QString ("color: %1;").arg (css (colors().text, alphas().muted));
}

inline QString editorStyle() {
    const Colors& c = colors();
    const Alphas& a = alphas();
    return QString ("QTextEdit { background: transparent; color: %1; border: none; padding: %2px;"
                    " selection-background-color: %3; selection-color: %1; }")
        .arg (css (c.text))
        .arg (metrics().editBorderWidth + 6)
        .arg (css (c.accent, a.selection));
}

inline QString viewStyle() {
    return QString ("QTextEdit { background: transparent; border: none; padding: %1px; }")
        .arg (metrics().editBorderWidth + 6);
}

inline QString wizardStyle() {
    const Colors& c = colors();
    const Alphas& a = alphas();
    return QString ("QTextEdit { background: transparent; color: %1; border: none; padding: %2px;"
                    " selection-background-color: %3; selection-color: %1; placeholder-text-color: %4; }")
        .arg (css (c.text))
        .arg (metrics().editBorderWidth + 6)
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

inline void loadBundledFonts() {
    static bool loaded = false;
    if (loaded)
        return;
    loaded = true;

    const char* files[] = {
        "TitilliumWeb-Regular.ttf", "TitilliumWeb-Italic.ttf", "TitilliumWeb-Bold.ttf", "TitilliumWeb-BoldItalic.ttf",
        "ChakraPetch-Regular.ttf"
    };
    bool textFont = false, labelFont = false;
    for (const char* file : files) {
        const QString name = QString::fromLatin1 (file);
        const int id = QFontDatabase::addApplicationFont (":/fonts/" + name);
        if (id < 0)
            continue;
        if (name.startsWith ("Titillium"))
            textFont = true;
        else if (name.startsWith ("Chakra"))
            labelFont = true;
    }
    if (textFont)
        fontsSlot().family = "Titillium Web";
    if (labelFont)
        fontsSlot().label = "Chakra Petch";
}

inline void applyTo (QApplication& app) {
    loadBundledFonts();
    app.setStyle (QStyleFactory::create ("Fusion"));
    app.setPalette (makePalette());
    app.setStyleSheet (appStyle());
}

}
