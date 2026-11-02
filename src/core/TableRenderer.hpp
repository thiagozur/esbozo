#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>
#include <QFile>
#include <QStringDecoder>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QFontMetricsF>
#include <cmath>

namespace TableRenderer {

inline QString decodeBytes (const QByteArray& raw) {
    QByteArray bytes = raw;
    if (bytes.startsWith ("\xEF\xBB\xBF"))
        bytes.remove (0, 3);

    QStringDecoder utf8 (QStringDecoder::Utf8);
    const QString text = utf8 (bytes);
    if (utf8.hasError())
        return QString::fromLatin1 (bytes);
    return text;
}

inline QChar detectDelimiter (const QString& text) {
    int semi = 0, comma = 0, tab = 0;
    bool inQuotes = false;
    for (QChar c : text) {
        if (c == u'"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes) {
            if (c == u'\n' || c == u'\r') {
                if (semi + comma + tab > 0) break;
                continue;
            }
            if (c == u';') ++semi;
            else if (c == u',') ++comma;
            else if (c == u'\t') ++tab;
        }
    }
    if (tab > semi && tab > comma) return QChar (u'\t');
    if (semi > 0 && semi >= comma) return QChar (u';');
    return QChar (u',');
}

inline QVector<QStringList> parseCsv (const QString& text) {
    QVector<QStringList> rows;
    const QChar delim = detectDelimiter (text);
    const int n = text.size();

    QStringList row;
    QString field;
    bool inQuotes = false;

    auto endField = [&] () {
        row.append (field.trimmed());
        field.clear();
    };
    auto endRow = [&] () {
        endField();
        const bool blank = (row.size() == 1 && row[0].isEmpty());
        if (!blank) rows.append (row);
        row.clear();
    };

    for (int i = 0; i < n; ++i) {
        const QChar c = text[i];
        if (inQuotes) {
            if (c == u'"') {
                if (i + 1 < n && text[i + 1] == u'"') { field += u'"'; ++i; }
                else inQuotes = false;
            } else {
                field += c;
            }
        } else if (c == u'"') {
            inQuotes = true;
        } else if (c == delim) {
            endField();
        } else if (c == u'\n' || c == u'\r') {
            if (c == u'\r' && i + 1 < n && text[i + 1] == u'\n') ++i;
            endRow();
        } else {
            field += c;
        }
    }
    if (!field.isEmpty() || !row.isEmpty()) endRow();
    return rows;
}

inline QVector<QStringList> loadCsvFile (const QString& path, bool* ok = nullptr) {
    QFile file (path);
    if (!file.open (QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return {};
    }
    QVector<QStringList> data = parseCsv (decodeBytes (file.readAll()));
    if (ok) *ok = !data.isEmpty();
    return data;
}

struct Style {
    QColor cell { Qt::white };
    QColor header { 240, 243, 246 };
    QColor grid { 210, 210, 210 };
    QColor headerRule { 210, 210, 210 };
    QColor headerText { 20, 20, 20 };
    QColor bodyText { 50, 50, 50 };
    QColor title { 34, 34, 34 };
    QColor empty { 150, 150, 150 };
    QString fontFamily { "Segoe UI" };
    double fontScale = 1.0;
};

namespace detail {

constexpr double kRowH = 30.0;
constexpr double kPadX = 10.0;
constexpr double kTitleH = 34.0;

enum CellKind { Normal = 0, Header = 1, RowLabel = 2 };

inline CellKind kindOf (int r, int c, bool hasHeader, bool hasRowNames) {
    if (hasHeader && r == 0) return Header;
    if (hasRowNames && c == 0) return RowLabel;
    return Normal;
}

inline QFont cellFont (CellKind kind, const QString& family, double scale) {
    QFont f (family);
    f.setPixelSize (qRound (14 * scale));
    f.setBold (kind == Header);
    f.setItalic (kind == RowLabel);
    return f;
}

inline bool isNumeric (const QString& s) {
    if (s.isEmpty()) return false;
    QString t = s;
    t.replace (u',', u'.');
    bool ok = false;
    t.toDouble (&ok);
    return ok;
}

inline int columnCount (const QVector<QStringList>& data) {
    int n = 0;
    for (const QStringList& row : data) n = qMax (n, static_cast<int> (row.size()));
    return n;
}

inline QVector<double> columnWidths (const QVector<QStringList>& data, bool hasHeader, bool hasRowNames, double available, const Style& style) {
    const int ncols = columnCount (data);
    QVector<double> w (ncols, 0.0);
    for (int r = 0; r < data.size(); ++r) {
        for (int c = 0; c < data[r].size(); ++c) {
            const QFontMetricsF fm (cellFont (kindOf (r, c, hasHeader, hasRowNames), style.fontFamily, style.fontScale));
            w[c] = qMax (w[c], fm.horizontalAdvance (data[r][c]) + 2 * kPadX * style.fontScale);
        }
    }
    double sum = 0.0;
    for (double x : w) sum += x;
    if (sum <= 0.0) return w;

    if (sum < available) {
        const double scale = qMax (sum, available * 0.6) / sum;
        for (double& x : w) x *= scale;
        return w;
    }

    const double shortLimit = 80.0 * style.fontScale;
    const double floorWidth = 40.0 * style.fontScale;
    double protectedSum = 0.0, flexSum = 0.0;
    int flexCount = 0;
    for (double x : w) {
        if (x <= shortLimit) {
            protectedSum += x;
        } else {
            flexSum += x;
            ++flexCount;
        }
    }

    const double rest = available - protectedSum;
    if (flexCount > 0 && rest >= floorWidth * flexCount) {
        for (double& x : w) {
            if (x > shortLimit)
                x = qMax (floorWidth, rest * x / flexSum);
        }
        double total = 0.0;
        for (double x : w) total += x;
        if (total > available) {
            double shrinkable = 0.0;
            for (double x : w)
                if (x > shortLimit) shrinkable += x - floorWidth;
            const double ratio = shrinkable > 0.0 ? qMin (1.0, (total - available) / shrinkable) : 0.0;
            for (double& x : w)
                if (x > shortLimit) x -= (x - floorWidth) * ratio;
        }
        return w;
    }

    const double scale = available / sum;
    for (double& x : w) x *= scale;
    return w;
}

}


inline int preferredHeight (const QVector<QStringList>& data, const QString& title = QString(), const Style& style = Style()) {
    if (data.isEmpty()) return 50;
    const double titleH = title.isEmpty() ? 0.0 : detail::kTitleH * style.fontScale;
    return static_cast<int> (std::ceil (data.size() * detail::kRowH * style.fontScale + titleH)) + 4;
}

inline QPixmap render (const QVector<QStringList>& data, bool hasHeaderRow, bool hasRowNames, const QString& title, int width, int height, qreal dpr = 1.0,
                       const Style& colors = Style())
{
    using namespace detail;
    const double kRowH = detail::kRowH * colors.fontScale;
    const double kPadX = detail::kPadX * colors.fontScale;
    const double kTitleH = detail::kTitleH * colors.fontScale;

    const int w = qMax (100, width);
    const int h = qMax (50, height);

    QImage img (qRound (w * dpr), qRound (h * dpr), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio (dpr);
    img.fill (Qt::transparent);

    QPainter p (&img);
    p.setRenderHint (QPainter::Antialiasing, true);
    p.setRenderHint (QPainter::TextAntialiasing, true);

    if (data.isEmpty()) {
        QFont f (colors.fontFamily);
        f.setPixelSize (qRound (14 * colors.fontScale));
        f.setItalic (true);
        p.setFont (f);
        p.setPen (colors.empty);
        p.drawText (QRectF (0, 0, w, h), Qt::AlignCenter, "(tabla vacía o sin datos)");
        p.end();
        return QPixmap::fromImage (img);
    }

    const int nrows = data.size();
    const int ncols = columnCount (data);
    const QVector<double> colW = columnWidths (data, hasHeaderRow, hasRowNames, w - 4.0, colors);
    double tableW = 0.0;
    for (double x : colW) tableW += x;

    const double x0 = (w - tableW) / 2.0;
    double y0 = 2.0;

    if (!title.isEmpty()) {
        QFont tf (colors.fontFamily);
        tf.setPixelSize (qRound (15 * colors.fontScale));
        tf.setBold (true);
        p.setFont (tf);
        p.setPen (colors.title);
        const QString shown = QFontMetricsF (tf).elidedText (title, Qt::ElideRight, w - 8.0);
        p.drawText (QRectF (0, y0, w, kTitleH - 4), Qt::AlignCenter, shown);
        y0 += kTitleH;
    }

    for (int r = 0; r < nrows; ++r) {
        double x = x0;
        for (int c = 0; c < ncols; ++c) {
            const QRectF cell (x, y0 + r * kRowH, colW[c], kRowH);
            p.fillRect (cell, (hasHeaderRow && r == 0) ? colors.header : colors.cell);
            x += colW[c];
        }
    }

    p.setPen (QPen (colors.grid, 1));
    const double tableH = nrows * kRowH;
    for (int r = 0; r <= nrows; ++r) {
        const double y = y0 + r * kRowH;
        p.drawLine (QPointF (x0, qMin (y, y0 + tableH - 0.5)), QPointF (x0 + tableW, qMin (y, y0 + tableH - 0.5)));
    }
    double gx = x0;
    for (int c = 0; c <= ncols; ++c) {
        const double x = qMin (gx, x0 + tableW - 0.5);
        p.drawLine (QPointF (x, y0), QPointF (x, y0 + tableH));
        if (c < ncols) gx += colW[c];
    }

    if (hasHeaderRow && nrows > 1 && colors.headerRule != colors.grid) {
        p.setPen (QPen (colors.headerRule, 1));
        p.drawLine (QPointF (x0, y0 + kRowH), QPointF (x0 + tableW, y0 + kRowH));
    }
    for (int r = 0; r < nrows; ++r) {
        double x = x0;
        for (int c = 0; c < ncols; ++c) {
            if (c < data[r].size() && !data[r][c].isEmpty()) {
                const CellKind kind = kindOf (r, c, hasHeaderRow, hasRowNames);
                const QFont f = cellFont (kind, colors.fontFamily, colors.fontScale);
                const QString text = data[r][c];
                p.setFont (f);
                p.setPen (kind == Header ? colors.headerText : colors.bodyText);

                Qt::Alignment align = Qt::AlignLeft;
                if (kind != Normal) align = Qt::AlignHCenter;
                else if (isNumeric (text)) align = Qt::AlignRight;

                const double avail = colW[c] - 2 * kPadX;
                const QString shown = QFontMetricsF (f).elidedText (text, Qt::ElideRight, avail);
                p.drawText (QRectF (x + kPadX, y0 + r * kRowH, avail, kRowH), align | Qt::AlignVCenter, shown);
            }
            x += colW[c];
        }
    }

    p.end();
    return QPixmap::fromImage (img);
}

}