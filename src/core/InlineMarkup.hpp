#pragma once

#include "Block.hpp"
#include <QString>
#include <QFont>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextCharFormat>

namespace InlineMarkup {

inline bool isRichType (BlockType t) {
    return t == BlockType::Paragraph || t == BlockType::Title || t == BlockType::Subtitle;
}

inline QString serialize (const QTextDocument* doc) {
    QString out;
    bool bold = false, italic = false;

    auto setState = [&] (bool b, bool i) {
        if (b != bold) {
            out += "**";
            bold = b;
        }
        if (i != italic) {
            out += "*";
            italic = i;
        }
    };

    for (QTextBlock blk = doc->begin(); blk.isValid(); blk = blk.next()) {
        for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            const QTextCharFormat f = frag.charFormat();
            setState (f.fontWeight() >= QFont::Bold, f.fontItalic());

            QString text = frag.text();
            text.replace (QChar (QChar::LineSeparator), QLatin1Char ('\n'));
            text.replace (QChar (QChar::Nbsp), QLatin1Char (' '));
            text.replace ("\\", "\\\\");
            text.replace ("*", "\\*");
            out += text;
        }
        if (blk.next().isValid())
            out += '\n';
    }
    setState (false, false);
    return out;
}

inline void load (QTextDocument* doc, const QString& markup) {
    doc->setUndoRedoEnabled (false);
    doc->clear();

    QTextCursor cur (doc);
    bool bold = false, italic = false;
    QString run;

    auto flush = [&] () {
        if (run.isEmpty())
            return;
        QTextCharFormat f;
        if (bold)
            f.setFontWeight (QFont::Bold);
        if (italic)
            f.setFontItalic (true);
        cur.insertText (run, f);
        run.clear();
    };

    const int n = markup.size();
    for (int i = 0; i < n; ++i) {
        const QChar c = markup[i];
        if (c == u'\\' && i + 1 < n) {
            run += markup[++i];
        } else if (c == u'*') {
            flush();
            if (i + 1 < n && markup[i + 1] == u'*') {
                bold = !bold;
                ++i;
            }
            else {
                italic = !italic;
            }
        } else {
            run += c;
        }
    }
    flush();

    doc->setUndoRedoEnabled (true);
}

inline QString toHtml (const QString& markup) {
    QString out;
    bool bold = false, italic = false;

    auto closeTags = [&] () {
        if (italic)
            out += "</i>";
        if (bold)
            out += "</b>";
    };
    auto openTags = [&] () {
        if (bold)
            out += "<b>";
        if (italic)
            out += "<i>";
    };
    auto put = [&] (QChar c) {
        if (c == u'&')
            out += "&amp;";
        else if (c == u'<')
            out += "&lt;";
        else if (c == u'>')
            out += "&gt;";
        else
            out += c;
    };

    const int n = markup.size();
    for (int i = 0; i < n; ++i) {
        const QChar c = markup[i];
        if (c == u'\\' && i + 1 < n) {
            put (markup[++i]);
        } else if (c == u'*') {
            closeTags();
            if (i + 1 < n && markup[i + 1] == u'*') {
                bold = !bold;
                ++i;
            }
            else {
                italic = !italic;
            }
            openTags();
        } else {
            put (c);
        }
    }
    closeTags();

    out.replace ("\n\n", "<br>&nbsp;<br>");
    out.replace ("\n", "<br>");
    return out;
}

}