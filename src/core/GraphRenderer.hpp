#pragma once

#include <QPixmap>
#include <QPainter>
#include <QFontMetrics>
#include <QColor>
#include <QVector>
#include <cmath>
#include <limits>

#include "GraphBlock.hpp"

namespace GraphRenderer {

inline QColor seriesColor (int i) {
    static const char* colors[] = { "#4682B4", "#E67E22", "#2E9E5B", "#C0392B", "#8E44AD", "#16A5A5", "#B8A100", "#7F8C8D" };
    return QColor (colors[i % 8]);
}

struct Axis {
    bool log = false;
    double lo = 0.0, hi = 1.0;
    QVector<double> ticks;

    bool valid (double v) const { return std::isfinite (v) && (!log || v > 0.0); }
    double toT (double v) const { return log ? std::log10 (v) : v; }
    double fracT (double t) const { return (t - lo) / (hi - lo); }
    double frac (double v) const { return fracT (toT (v)); }
    QString label (double t) const { return QString::number (log ? std::pow (10.0, t) : t, 'g', 4); }
};

inline Axis makeAxis (const QVector<double>& vals, bool log) {
    Axis a;
    a.log = log;

    double mn = std::numeric_limits<double>::infinity();
    double mx = -std::numeric_limits<double>::infinity();
    for (double v : vals) {
        if (!a.valid (v)) continue;
        double t = a.toT (v);
        mn = qMin (mn, t);
        mx = qMax (mx, t);
    }
    if (mn > mx) { mn = 0.0; mx = 1.0; }

    if (log) {
        a.lo = std::floor (mn);
        a.hi = std::ceil (mx);
        if (a.hi - a.lo < 1.0) a.hi = a.lo + 1.0;
        int step = qMax (1, static_cast<int> (a.hi - a.lo) / 6);
        for (int e = static_cast<int> (a.lo); e <= static_cast<int> (a.hi); e += step)
            a.ticks.append (e);
    } else {
        if (mn == mx) { mn -= 1.0; mx += 1.0; }
        double rawStep = (mx - mn) / 6.0;
        double mag = std::pow (10.0, std::floor (std::log10 (rawStep)));
        double norm = rawStep / mag;
        double step = (norm < 1.5 ? 1.0 : norm < 3.0 ? 2.0 : norm < 7.0 ? 5.0 : 10.0) * mag;

        a.lo = std::floor (mn / step) * step;
        a.hi = std::ceil (mx / step) * step;
        int n = static_cast<int> (std::round ((a.hi - a.lo) / step));
        for (int i = 0; i <= n; ++i)
            a.ticks.append (a.lo + i * step);
    }
    return a;
}

inline QPixmap render (const GraphBlock& g, int width, int height, qreal dpr = 1.0) {
    QPixmap pm (QSize (width, height) * dpr);
    pm.setDevicePixelRatio (dpr);
    pm.fill (Qt::white);

    QPainter p (&pm);
    p.setRenderHints (QPainter::Antialiasing | QPainter::TextAntialiasing);

    const QVector<double>& xs = g.getSeriesX().values;
    const QVector<DataSeries>& ys = g.getSeriesYList();
    const bool isLine = g.getGraphType() == GraphType::Line;
    const bool multi = ys.size() > 1;

    QVector<double> allY;
    for (const DataSeries& s : ys) allY += s.values;

    Axis ax = makeAxis (xs, g.getScaleX() == AxisScale::Log);
    Axis ay = makeAxis (allY, g.getScaleY() == AxisScale::Log);

    QFont tickFont ("Segoe UI", 8), labelFont ("Segoe UI", 9), titleFont ("Segoe UI", 11, QFont::Bold);
    QFontMetrics tfm (tickFont), lfm (labelFont), hfm (titleFont);

    const int margin = 12;
    int top = margin;
    if (!g.getDescription().isEmpty()) top += hfm.height() + 10;

    QVector<QPoint> legendPos;
    int legendH = 0;
    if (multi) {
        const int rowH = lfm.height() + 4;
        int x = margin, row = 0;
        for (const DataSeries& s : ys) {
            int w = 22 + lfm.horizontalAdvance (s.name) + 18;
            if (x + w > width - margin && x > margin) { x = margin; ++row; }
            legendPos.append (QPoint (x, row * rowH));
            x += w;
        }
        legendH = (row + 1) * rowH + 6;
    }

    const bool yLabelOnSide = !multi && !ys.isEmpty() && !ys[0].name.isEmpty();
    const bool hasXLabel = !g.getSeriesX().name.isEmpty();

    int yTickW = 0;
    for (double t : ay.ticks) yTickW = qMax (yTickW, tfm.horizontalAdvance (ay.label (t)));

    int left = margin + (yLabelOnSide ? lfm.height() + 6 : 0) + yTickW + 8;
    int right = width - margin - 8;
    int bottom = height - margin - legendH - (hasXLabel ? lfm.height() + 4 : 0) - (tfm.height() + 6);

    QRect plot (QPoint (left, top), QPoint (right, bottom));
    if (plot.width() < 40 || plot.height() < 40) return pm;

    auto px = [&] (double v) { return plot.left() + ax.frac (v) * plot.width(); };
    auto py = [&] (double v) { return plot.bottom() - ay.frac (v) * plot.height(); };

    if (!g.getDescription().isEmpty()) {
        p.setFont (titleFont);
        p.setPen (QColor ("#222222"));
        p.drawText (QRectF (margin, margin, width - 2 * margin, hfm.height()), Qt::AlignCenter, hfm.elidedText (g.getDescription(), Qt::ElideRight, width - 2 * margin));
    }

    p.setFont (tickFont);
    for (double t : ax.ticks) {
        double x = plot.left() + ax.fracT (t) * plot.width();
        p.setPen (QPen (QColor ("#EEEEEE"), 1));
        p.drawLine (QPointF (x, plot.top()), QPointF (x, plot.bottom()));
        p.setPen (QColor ("#555555"));
        p.drawLine (QPointF (x, plot.bottom()), QPointF (x, plot.bottom() + 4));
        p.drawText (QRectF (x - 30, plot.bottom() + 5, 60, tfm.height()), Qt::AlignHCenter | Qt::AlignTop, ax.label (t));
    }
    for (double t : ay.ticks) {
        double y = plot.bottom() - ay.fracT (t) * plot.height();
        p.setPen (QPen (QColor ("#EEEEEE"), 1));
        p.drawLine (QPointF (plot.left(), y), QPointF (plot.right(), y));
        p.setPen (QColor ("#555555"));
        p.drawLine (QPointF (plot.left() - 4, y), QPointF (plot.left(), y));
        p.drawText (QRectF (plot.left() - yTickW - 8, y - tfm.height() / 2.0, yTickW + 3, tfm.height()), Qt::AlignRight | Qt::AlignVCenter, ay.label (t));
    }

    p.setPen (QPen (QColor ("#AAAAAA"), 1));
    p.setBrush (Qt::NoBrush);
    p.drawRect (plot);

    bool anyData = false;
    p.save();
    p.setClipRect (plot.adjusted (-1, -1, 1, 1));

    for (int s = 0; s < ys.size(); ++s) {
        const QVector<double>& yv = ys[s].values;
        const int n = qMin (xs.size(), yv.size());
        const QColor color = seriesColor (s);

        QVector<QPointF> pts;
        QPointF prev;
        bool havePrev = false;

        p.setPen (QPen (color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (int i = 0; i < n; ++i) {
            if (!ax.valid (xs[i]) || !ay.valid (yv[i])) { havePrev = false; continue; }
            QPointF pt (px (xs[i]), py (yv[i]));
            pts.append (pt);
            if (isLine && havePrev) p.drawLine (prev, pt);
            prev = pt;
            havePrev = true;
        }
        if (!pts.isEmpty()) anyData = true;

        p.setBrush (color);
        p.setPen (QPen (Qt::white, 1));
        const double r = isLine ? 2.5 : 3.5;
        if (!isLine || pts.size() <= 40) {
            for (const QPointF& pt : pts) p.drawEllipse (pt, r, r);
        }
    }
    p.restore();

    if (!anyData) {
        p.setFont (labelFont);
        p.setPen (QColor ("#888888"));
        p.drawText (plot, Qt::AlignCenter, "Sin datos para graficar");
    }

    p.setFont (labelFont);
    p.setPen (QColor ("#222222"));
    if (hasXLabel)
        p.drawText (QRectF (plot.left(), plot.bottom() + 5 + tfm.height() + 4, plot.width(), lfm.height()), Qt::AlignCenter, g.getSeriesX().name);
    if (yLabelOnSide) {
        p.save();
        p.translate (margin, plot.center().y());
        p.rotate (-90);
        p.drawText (QRectF (-plot.height() / 2.0, 0, plot.height(), lfm.height()), Qt::AlignCenter, ys[0].name);
        p.restore();
    }

    if (multi) {
        const int legendTop = height - margin - legendH + 4;
        for (int s = 0; s < ys.size(); ++s) {
            QPoint o = legendPos[s] + QPoint (0, legendTop);
            const double midY = o.y() + lfm.height() / 2.0;
            p.setPen (QPen (seriesColor (s), 2));
            p.setBrush (seriesColor (s));
            if (isLine) p.drawLine (QPointF (o.x(), midY), QPointF (o.x() + 16, midY));
            p.drawEllipse (QPointF (o.x() + 8, midY), 3.5, 3.5);

            p.setPen (QColor ("#222222"));
            p.drawText (QRectF (o.x() + 22, o.y(), lfm.horizontalAdvance (ys[s].name) + 4, lfm.height()), Qt::AlignLeft | Qt::AlignVCenter, ys[s].name);
        }
    }

    return pm;
}

}