#pragma once

#include <QFont>
#include <QString>

#include "core/Block.hpp"

namespace Theme {

struct Colors {
    QString textTitle = "#222222";
    QString textSubtitle = "#444444";
    QString textBody = "#555555";
    QString textMuted = "#888888";
    QString textError = "#CC0000";
    QString statusText = "gray";

    QString cardBackground = "#FAFAFA";
    QString cardBorder = "#E0E0E0";

    QString editBackground = "#F0F8FF";
    QString editBorder = "#4682B4";

    QString wizardBackground = "#FEF9E7";
    QString wizardBorder = "#E67E22";

    QString dialogBackground = "#Fdfdfd";
    QString dialogText = "#333333";
    QString buttonBackground = "#4682B4";
    QString buttonText = "white";
    QString buttonHover = "#3b6e9b";
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
    int cardBorderWidth = 1;
    int cardChrome() const { return 2 * (cardPadding + cardBorderWidth); }
};

inline const Colors& colors() {
    static const Colors c;
    return c;
}

inline const Fonts& fonts() {
    static const Fonts f;
    return f;
}

inline const Metrics& metrics() {
    static const Metrics m;
    return m;
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
    const QString& color = type == BlockType::Title ? c.textTitle
                         : type == BlockType::Subtitle ? c.textSubtitle
                         : c.textBody;
    return QString ("QLabel { color: %1; padding: 6px; }").arg (color);
}

inline QString cardStyle() {
    const Colors& c = colors();
    const Metrics& m = metrics();
    return QString ("QLabel { padding: %1px; background-color: %2; border: %3px solid %4; border-radius: 4px; }")
        .arg (m.cardPadding)
        .arg (c.cardBackground)
        .arg (m.cardBorderWidth)
        .arg (c.cardBorder);
}

inline QString mutedLabelStyle() {
    return QString ("QLabel { color: %1; font-style: italic; padding: 6px; }").arg (colors().textMuted);
}

inline QString errorLabelStyle() {
    return QString ("QLabel { color: %1; font-weight: bold; padding: 6px; }").arg (colors().textError);
}

inline QString statusStyle() {
    return QString ("color: %1; font-style: italic;").arg (colors().statusText);
}

inline QString editorStyle() {
    const Colors& c = colors();
    return QString ("QTextEdit { background-color: %1; border: 2px solid %2; padding: 6px; border-radius: 4px; }")
        .arg (c.editBackground)
        .arg (c.editBorder);
}

inline QString wizardStyle() {
    const Colors& c = colors();
    return QString ("QTextEdit { background-color: %1; border: 2px solid %2; padding: 6px; border-radius: 4px; }")
        .arg (c.wizardBackground)
        .arg (c.wizardBorder);
}

inline QString dialogStyle() {
    const Colors& c = colors();
    const Fonts& f = fonts();
    return QString ("QDialog { background-color: %1; }"
                    "QLabel { color: %2; font-family: '%3'; }"
                    "QPushButton { background-color: %4; color: %5; border-radius: 4px; padding: 6px 12px; font-family: '%3'; }"
                    "QPushButton:hover { background-color: %6; }")
        .arg (c.dialogBackground)
        .arg (c.dialogText)
        .arg (f.family)
        .arg (c.buttonBackground)
        .arg (c.buttonText)
        .arg (c.buttonHover);
}

}
