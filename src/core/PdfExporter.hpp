#pragma once

#include "Document.hpp"
#include "GraphBlock.hpp"
#include "TableBlock.hpp"
#include "GraphRenderer.hpp"
#include "EquationRenderer.hpp"
#include "InlineMarkup.hpp"

#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QRawFont>
#include <QGlyphRun>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextBlock>
#include <QTextOption>
#include <QImage>
#include <QPixmap>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace PdfExporter {

struct Style {
    QString fontFamily = "Vollkorn";

    double bodyPt = 12.0;
    double titlePt = 16.0;
    double subtitlePt = 14.0;
    double tablePt = 12.0;
    double equationPt = 13.0;
    double pageNumberPt = 10.0;

    double imageWidth = 0.50;
    double graphWidth = 0.75;
    double tableWidth = 0.75;
    double equationWidth = 0.75;

    double marginPt = 72.0;

    double textAfter = 8.0;
    double titleBefore = 12.0, titleAfter = 8.0;
    double subtitleBefore = 6.0, subtitleAfter = 6.0;
    double figureBefore = 8.0, figureAfter = 12.0;

    int imageDpi = 200;
    int graphRasterScale = 4;
    bool showTableTitle = true;
    bool pageNumbers = true;

    bool boldAsOutlines = true;
    bool allTextAsOutlines = false;
};

struct Result {
    bool ok = false;
    QString message;
};

namespace detail {

constexpr double U = 8.0;

using PaintFn = std::function<void (QPainter&, double, double)>;

struct Row {
    double h = 0.0;
    PaintFn paint;
};

struct Item {
    std::vector<Row> rows;
    std::vector<char> breakAfter;
    double before = 0.0, after = 0.0;
    bool keepWithNext = false;
};

struct Placement {
    int item, r0, r1;
    double y;
};
using PageLayout = std::vector<Placement>;

struct TextMode {
    bool boldOutlines = true;
    bool allOutlines = false;
};

inline TextMode modeOf (const Style& st) {
    return TextMode { st.boldAsOutlines, st.allTextAsOutlines };
}

inline QFont makeFont (const Style& st, double pt, bool bold, bool italic, bool underline) {
    QFont f;
    f.setFamilies (QStringList { st.fontFamily, "Times New Roman", "Liberation Serif", "serif" });
    f.setPixelSize (qRound (pt * U));
    f.setBold (bold);
    f.setItalic (italic);
    f.setUnderline (underline);
    f.setHintingPreference (QFont::PreferNoHinting);
    return f;
}

inline void drawTextLine (QPainter& p, const QTextLine& line, double x, double y, const TextMode& mode, const QFont& font, bool underline)
{
    const QPointF origin (x, y - line.y());

    if (!mode.boldOutlines && !mode.allOutlines) {
        p.setPen (Qt::black);
        line.draw (&p, origin);
        return;
    }

    const QList<QGlyphRun> runs = line.glyphRuns();
    for (const QGlyphRun& run : runs) {
        const QRawFont raw = run.rawFont();
        if (mode.allOutlines || raw.weight() >= QFont::DemiBold) {
            const QList<quint32> indexes = run.glyphIndexes();
            const QList<QPointF> positions = run.positions();
            QPainterPath path;
            path.setFillRule (Qt::WindingFill);
            for (int i = 0; i < indexes.size() && i < positions.size(); ++i)
                path.addPath (raw.pathForGlyph (indexes[i]).translated (origin + positions[i]));
            p.fillPath (path, Qt::black);
        } else {
            p.setPen (Qt::black);
            p.drawGlyphRun (origin, run);
        }
    }

    if (underline) {
        const QFontMetricsF fm (font);
        p.fillRect (QRectF (x + line.cursorToX (line.textStart()), y + line.ascent() + fm.underlinePos(), line.naturalTextWidth(), qMax (1.0, fm.lineWidth())), Qt::black);
    }
}

struct StringLayout {
    std::shared_ptr<QTextLayout> layout;
    double height = 0.0;
};

inline StringLayout layoutString (const QString& text, const QFont& font, double width, Qt::Alignment align) {
    StringLayout s;
    s.layout = std::make_shared<QTextLayout> (text, font);
    QTextOption opt;
    opt.setAlignment (align);
    opt.setWrapMode (QTextOption::WrapAtWordBoundaryOrAnywhere);
    s.layout->setTextOption (opt);

    s.layout->beginLayout();
    double y = 0.0;
    while (true) {
        QTextLine line = s.layout->createLine();
        if (!line.isValid()) break;
        line.setLineWidth (width);
        line.setPosition (QPointF (0.0, y));
        y += line.height();
    }
    s.layout->endLayout();

    s.height = y > 0.0 ? y : QFontMetricsF (font).lineSpacing();
    return s;
}

inline double textWidth (const QString& text, const QFont& font) {
    QTextLayout layout (text, font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    double w = 0.0;
    if (line.isValid()) {
        line.setLineWidth (1e7);
        w = line.naturalTextWidth();
    }
    layout.endLayout();
    return w;
}

inline void drawString (QPainter& p, const StringLayout& s, double x, double y, const TextMode& mode, const QFont& font) {
    for (int i = 0; i < s.layout->lineCount(); ++i) {
        const QTextLine line = s.layout->lineAt (i);
        drawTextLine (p, line, x, y + line.y(), mode, font, false);
    }
}

inline Item makeTextItem (const QString& markup, const Style& st, double pt, bool bold, bool italic, bool underline, Qt::Alignment align, double beforePt, double afterPt, bool keepWithNext, double widthU)
{
    auto doc = std::make_shared<QTextDocument>();
    InlineMarkup::load (doc.get(), markup);
    doc->setDocumentMargin (0);
    const QFont font = makeFont (st, pt, bold, italic, underline);
    doc->setDefaultFont (font);
    QTextOption opt;
    opt.setAlignment (align);
    opt.setWrapMode (QTextOption::WordWrap);
    doc->setDefaultTextOption (opt);
    doc->setTextWidth (widthU);
    (void) doc->size();

    const TextMode mode = modeOf (st);

    Item item;
    item.before = beforePt * U;
    item.after = afterPt * U;
    item.keepWithNext = keepWithNext;

    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        QTextLayout* lay = b.layout();
        for (int i = 0; i < lay->lineCount(); ++i) {
            const QTextLine ln = lay->lineAt (i);
            Row row;
            row.h = ln.height();
            row.paint = [doc, ln, mode, font, underline] (QPainter& p, double x, double y) {
                drawTextLine (p, ln, x, y, mode, font, underline);
            };
            item.rows.push_back (std::move (row));
        }
    }

    const int n = static_cast<int> (item.rows.size());
    item.breakAfter.assign (n, 0);
    for (int i = 0; i + 1 < n; ++i)
        item.breakAfter[i] = (i + 1 >= 2 && n - (i + 1) >= 2) ? 1 : 0;
    return item;
}

inline Item makeAtomicItem (double h, PaintFn paint, const Style& st) {
    Item item;
    item.before = st.figureBefore * U;
    item.after = st.figureAfter * U;
    Row r;
    r.h = h;
    r.paint = std::move (paint);
    item.rows.push_back (std::move (r));
    item.breakAfter.assign (1, 0);
    return item;
}

inline Item makeImageItem (const QString& path, const Style& st, double contentW, double contentH) {
    QImage img (path);
    if (img.isNull()) {
        return makeTextItem ("[Imagen no encontrada: " + path + "]", st, st.bodyPt, false, true, false, Qt::AlignHCenter, st.figureBefore, st.figureAfter, false, contentW);
    }

    double w = contentW * st.imageWidth;
    double h = w * img.height() / img.width();
    const double maxH = contentH * 0.9;
    if (h > maxH) {
        w *= maxH / h;
        h = maxH;
    }

    const double inches = (w / U) / 72.0;
    const int maxPx = qMax (1, static_cast<int> (inches * st.imageDpi));
    if (img.width() > maxPx)
        img = img.scaledToWidth (maxPx, Qt::SmoothTransformation);

    return makeAtomicItem (h, [img, w, h, contentW] (QPainter& p, double x, double y) {
        p.drawImage (QRectF (x + (contentW - w) / 2.0, y, w, h), img);
    }, st);
}

inline Item makeGraphItem (const GraphBlock& g, const Style& st, double contentW) {
    const double wU = contentW * st.graphWidth;
    const double hU = wU * 13.0 / 20.0;

    const int pxW = qRound ((wU / U) * 96.0 / 72.0);
    const int pxH = pxW * 13 / 20;
    QImage img = GraphRenderer::render (g, pxW, pxH, st.graphRasterScale).toImage();
    img.setDevicePixelRatio (1.0);

    return makeAtomicItem (hU, [img, wU, hU, contentW] (QPainter& p, double x, double y) {
        p.drawImage (QRectF (x + (contentW - wU) / 2.0, y, wU, hU), img);
    }, st);
}

inline Item makeEquationItem (const QString& latex, int number, const Style& st, double contentW) {
    const double areaW = contentW * st.equationWidth;
    const double fontU = st.equationPt * U;
    const QSizeF sz = EquationRenderer::measureScaled (latex, fontU, areaW);
    const double pad = 0.6 * fontU;
    const double h = qMax (sz.height() + 2 * pad, 2.2 * fontU);

    const TextMode mode = modeOf (st);
    const QFont numFont = makeFont (st, st.bodyPt, false, false, false);
    const StringLayout numLayout = layoutString (QString ("(%1)").arg (number), numFont, contentW, Qt::AlignRight);

    return makeAtomicItem (h, [=] (QPainter& p, double x, double y) {
        EquationRenderer::paintScaled (p, latex, QRectF (x + (contentW - areaW) / 2.0, y, areaW, h), fontU);
        drawString (p, numLayout, x, y + (h - numLayout.height) / 2.0, mode, numFont);
    }, st);
}

inline Item makeTableItem (const TableBlock& tb, const Style& st, double contentW, double contentH) {
    const QVector<QStringList> data = tb.getData();
    if (data.isEmpty()) return Item();

    const bool hasHeader = tb.hasHeaderRow();
    const bool hasRowNames = tb.hasRowNames();
    const int nrows = data.size();
    int ncols = 0;
    for (const QStringList& r : data) ncols = qMax (ncols, static_cast<int> (r.size()));
    if (ncols == 0) return Item();

    double tableW = contentW * st.tableWidth;
    const double padX = 5.4 * U, padY = 3.0 * U;
    const double slack = 0.5 * U;
    const TextMode mode = modeOf (st);

    const QFont normalFont = makeFont (st, st.tablePt, false, false, false);
    const QFont boldFont = makeFont (st, st.tablePt, true, false, false);
    auto isBold = [=] (int r, int c) { return (hasHeader && r == 0) || (hasRowNames && c == 0); };
    auto cellText = [data] (int r, int c) { return c < data[r].size() ? data[r][c] : QString(); };

    QVector<double> nat (ncols, 2 * padX), minW (ncols, 2 * padX);
    for (int r = 0; r < nrows; ++r) {
        for (int c = 0; c < ncols; ++c) {
            const QFont& font = isBold (r, c) ? boldFont : normalFont;
            const QString text = cellText (r, c);
            nat[c] = qMax (nat[c], textWidth (text, font) + 2 * padX + slack);
            for (const QString& word : text.split (QRegularExpression ("\\s+"), Qt::SkipEmptyParts))
                minW[c] = qMax (minW[c], textWidth (word, font) + 2 * padX + slack);
        }
    }
    double sumNat = 0.0, sumMin = 0.0;
    for (double v : nat) sumNat += v;
    for (double v : minW) sumMin += v;
    if (sumMin > tableW) tableW = std::min (contentW, sumMin);

    QVector<double> colW (ncols, 0.0);
    if (sumNat <= tableW) {
        for (int c = 0; c < ncols; ++c) colW[c] = tableW * nat[c] / sumNat;
    } else {
        QVector<char> fixedCol (ncols, 0);
        double remaining = tableW;
        int flex = ncols;
        bool changed = true;
        while (changed && flex > 0) {
            changed = false;
            const double share = remaining / flex;
            for (int c = 0; c < ncols; ++c) {
                if (!fixedCol[c] && nat[c] <= share) {
                    fixedCol[c] = 1;
                    colW[c] = nat[c];
                    remaining -= nat[c];
                    --flex;
                    changed = true;
                }
            }
        }
        double flexNat = 0.0;
        for (int c = 0; c < ncols; ++c) if (!fixedCol[c]) flexNat += nat[c];
        for (int c = 0; c < ncols; ++c) if (!fixedCol[c]) colW[c] = remaining * nat[c] / flexNat;
    }
    for (int pass = 0; pass < 4; ++pass) {
        double deficit = 0.0, slack = 0.0;
        for (int c = 0; c < ncols; ++c) {
            if (colW[c] < minW[c]) deficit += minW[c] - colW[c];
            else slack += colW[c] - minW[c];
        }
        if (deficit <= 0.01 || slack <= 0.01) break;
        const double take = qMin (1.0, deficit / slack);
        for (int c = 0; c < ncols; ++c) {
            if (colW[c] < minW[c]) colW[c] = minW[c];
            else colW[c] -= (colW[c] - minW[c]) * take;
        }
    }
    double sumW = 0.0;
    for (double v : colW) sumW += v;
    if (sumW > tableW * 1.0001)
        for (double& v : colW) v *= tableW / sumW;

    std::vector<std::vector<StringLayout>> cells (nrows, std::vector<StringLayout> (ncols));
    QVector<double> rowH (nrows);
    const double minLine = QFontMetricsF (normalFont).lineSpacing();
    for (int r = 0; r < nrows; ++r) {
        double mh = minLine;
        for (int c = 0; c < ncols; ++c) {
            const bool bold = isBold (r, c);
            cells[r][c] = layoutString (cellText (r, c), bold ? boldFont : normalFont, colW[c] - 2 * padX, bold ? Qt::AlignHCenter : Qt::AlignLeft);
            mh = qMax (mh, cells[r][c].height);
        }
        rowH[r] = mh + 2 * padY;
    }

    Item item;
    item.before = st.figureBefore * U;
    item.after = st.figureAfter * U;
    const double x0 = (contentW - tableW) / 2.0;

    const bool hasTitle = st.showTableTitle && !tb.getTitle().trimmed().isEmpty();
    if (hasTitle) {
        const StringLayout titleLayout = layoutString (tb.getTitle().trimmed(), boldFont, tableW, Qt::AlignHCenter);
        Row t;
        t.h = titleLayout.height + 2 * padY;
        t.paint = [=] (QPainter& p, double x, double y) {
            drawString (p, titleLayout, x + x0, y + padY, mode, boldFont);
        };
        item.rows.push_back (std::move (t));
    }

    for (int r = 0; r < nrows; ++r) {
        Row row;
        row.h = rowH[r];
        const double h = rowH[r];
        row.paint = [=] (QPainter& p, double x, double y) {
            double cx = x + x0;
            for (int c = 0; c < ncols; ++c) {
                p.setPen (QPen (Qt::black, 0.5 * U));
                p.setBrush (Qt::NoBrush);
                p.drawRect (QRectF (cx, y, colW[c], h));
                drawString (p, cells[r][c], cx + padX, y + padY, mode, isBold (r, c) ? boldFont : normalFont);
                cx += colW[c];
            }
        };
        item.rows.push_back (std::move (row));
    }

    double total = 0.0;
    for (const Row& r : item.rows) total += r.h;
    const int n = static_cast<int> (item.rows.size());
    item.breakAfter.assign (n, 0);
    if (total > 0.35 * contentH) {
        const int firstDataRow = hasTitle ? 1 : 0;
        for (int i = 0; i + 1 < n; ++i) {
            const bool afterTitle = hasTitle && i == 0;
            const bool afterHeader = hasHeader && i == firstDataRow;
            item.breakAfter[i] = (afterTitle || afterHeader) ? 0 : 1;
        }
    }
    return item;
}

inline double firstChunkHeight (const Item& it) {
    double h = 0.0;
    for (size_t i = 0; i < it.rows.size(); ++i) {
        h += it.rows[i].h;
        if (i + 1 == it.rows.size() || it.breakAfter[i]) break;
    }
    return h;
}

inline double chunkNeeded (const std::vector<Item>& items, size_t i) {
    double need = firstChunkHeight (items[i]);
    if (items[i].keepWithNext && i + 1 < items.size())
        need += std::max (items[i].after, items[i + 1].before) + chunkNeeded (items, i + 1);
    return need;
}

inline std::vector<PageLayout> paginate (const std::vector<Item>& items, double H) {
    std::vector<PageLayout> pages (1);
    double y = 0.0;
    double prevAfter = 0.0;

    auto newPage = [&] () {
        pages.emplace_back();
        y = 0.0;
    };

    for (size_t i = 0; i < items.size(); ++i) {
        const Item& it = items[i];
        const size_t n = it.rows.size();
        if (n == 0) continue;

        double gap = pages.back().empty() ? 0.0 : std::max (prevAfter, it.before);
        if (!pages.back().empty() && y + gap + chunkNeeded (items, i) > H) {
            newPage();
            gap = 0.0;
        }
        y += gap;

        size_t r = 0;
        while (r < n) {
            double h = 0.0, bestH = 0.0;
            size_t best = r;
            for (size_t k = r; k < n; ++k) {
                if (y + h + it.rows[k].h > H) break;
                h += it.rows[k].h;
                if (k + 1 == n || it.breakAfter[k]) {
                    best = k + 1;
                    bestH = h;
                }
            }

            if (best == r) {
                if (!pages.back().empty()) {
                    newPage();
                    continue;
                }
                size_t k = r;
                double hh = 0.0;
                while (k < n && (k == r || y + hh + it.rows[k].h <= H)) {
                    hh += it.rows[k].h;
                    ++k;
                }
                best = k;
                bestH = hh;
            }

            pages.back().push_back ({ static_cast<int> (i), static_cast<int> (r), static_cast<int> (best), y });
            y += bestH;
            r = best;
            if (r < n) newPage();
        }
        prevAfter = it.after;
    }
    return pages;
}

}

inline Result exportDocument (Document& doc, const QString& path, const Style& st = Style()) {
    using namespace detail;
    Result res;

    QFile probe (path);
    if (!probe.open (QIODevice::WriteOnly)) {
        res.message = "No se pudo escribir en: " + path;
        return res;
    }
    probe.close();

    const QSizeF pageSizePt = QPageSize (QPageSize::A4).size (QPageSize::Point);
    const double contentW = (pageSizePt.width() - 2 * st.marginPt) * U;
    const double contentH = (pageSizePt.height() - 2 * st.marginPt) * U;
    const double marginU = st.marginPt * U;

    std::vector<Item> items;
    QString docTitle;
    int equationNumber = 0;

    for (size_t i = 0; i < doc.getBlockCount(); ++i) {
        const Block* block = doc.getBlock (i);
        if (!block) continue;
        const QString content = block->getContent();

        switch (block->getType()) {
            case BlockType::Title: {
                if (content.trimmed().isEmpty()) break;
                if (docTitle.isEmpty()) {
                    QTextDocument tmp;
                    InlineMarkup::load (&tmp, content);
                    docTitle = tmp.toPlainText().trimmed();
                }
                items.push_back (makeTextItem (content, st, st.titlePt, true, false, true, Qt::AlignHCenter, st.titleBefore, st.titleAfter, true, contentW));
                break;
            }
            case BlockType::Subtitle:
                if (content.trimmed().isEmpty()) break;
                items.push_back (makeTextItem (content, st, st.subtitlePt, false, true, false, Qt::AlignLeft, st.subtitleBefore, st.subtitleAfter, true, contentW));
                break;
            case BlockType::Paragraph:
                if (content.trimmed().isEmpty()) break;
                items.push_back (makeTextItem (content, st, st.bodyPt, false, false, false, Qt::AlignJustify, 0.0, st.textAfter, false, contentW));
                break;
            case BlockType::Image:
                if (content.isEmpty()) break;
                items.push_back (makeImageItem (content, st, contentW, contentH));
                break;
            case BlockType::Graph:
                if (const GraphBlock* g = dynamic_cast<const GraphBlock*> (block)) {
                    if (!g->getSeriesX().values.isEmpty() && !g->getSeriesYList().isEmpty())
                        items.push_back (makeGraphItem (*g, st, contentW));
                }
                break;
            case BlockType::Equation:
                if (content.trimmed().isEmpty()) break;
                items.push_back (makeEquationItem (content, ++equationNumber, st, contentW));
                break;
            case BlockType::Table:
                if (const TableBlock* t = dynamic_cast<const TableBlock*> (block)) {
                    Item item = makeTableItem (*t, st, contentW, contentH);
                    if (!item.rows.empty()) items.push_back (std::move (item));
                }
                break;
            default:
                break;
        }
    }

    const std::vector<PageLayout> pages = paginate (items, contentH);

    QPdfWriter writer (path);
    writer.setPageSize (QPageSize (QPageSize::A4));
    writer.setPageMargins (QMarginsF (0, 0, 0, 0));
    writer.setResolution (1200);
    writer.setCreator ("Esbozo");
    if (!docTitle.isEmpty()) writer.setTitle (docTitle);

    QPainter p (&writer);
    if (!p.isActive()) {
        res.message = "No se pudo crear el PDF";
        return res;
    }
    p.setRenderHints (QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    const double scale = (1200.0 / 72.0) / U;
    const TextMode mode = modeOf (st);
    const QFont numberFont = makeFont (st, st.pageNumberPt, false, false, false);

    for (size_t pg = 0; pg < pages.size(); ++pg) {
        if (pg > 0) writer.newPage();

        p.resetTransform();
        p.scale (scale, scale);
        p.translate (marginU, marginU);

        for (const Placement& pl : pages[pg]) {
            const Item& it = items[pl.item];
            double y = pl.y;
            for (int r = pl.r0; r < pl.r1; ++r) {
                p.save();
                if (it.rows[r].paint) it.rows[r].paint (p, 0.0, y);
                p.restore();
                y += it.rows[r].h;
            }
        }

        if (st.pageNumbers) {
            const StringLayout number = layoutString (QString::number (static_cast<int> (pg) + 1), numberFont, contentW, Qt::AlignHCenter);
            drawString (p, number, 0.0, contentH + (marginU - number.height) / 2.0, mode, numberFont);
        }
    }
    p.end();

    res.ok = true;
    if (!QFontDatabase::hasFamily (st.fontFamily))
        res.message = QString ("La fuente \"%1\" no está instalada; se usó una fuente alternativa.").arg (st.fontFamily);
    return res;
}

}