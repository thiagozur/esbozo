#pragma once

#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QString>
#include <QWidget>

#include "ui/Theme.hpp"

namespace Backdrop {

struct Style {
    bool beams = true;
    bool dots = true;
    bool grain = true;
    bool vignette = true;

    double beamAlpha = 0.10;
    double dotAlpha = 0.10;
    int dotSpacing = 40;
    int grainAlpha = 16;
    double vignetteStrength = 0.45;
};

inline Style& currentStyle() {
    static Style style;
    return style;
}

inline Style preset (const QString& name) {
    Style s;
    if (name == "flat") {
        s.beams = s.dots = s.grain = s.vignette = false;
    } else if (name == "dots") {
        s.beams = false;
        s.grain = false;
        s.vignette = false;
    } else if (name == "beams") {
        s.dots = false;
        s.grain = false;
    } else if (name == "grain") {
        s.beams = false;
        s.dots = false;
        s.vignetteStrength = 0.35;
    } else if (name == "subtle") {
        s.beamAlpha = 0.065;
        s.dotAlpha = 0.07;
        s.grainAlpha = 11;
        s.vignetteStrength = 0.38;
    }
    return s;
}

inline QImage grainTile (int size, int maxAlpha) {
    QImage tile (size, size, QImage::Format_ARGB32);
    QRandomGenerator rng (20240607);
    for (int y = 0; y < size; ++y) {
        QRgb* line = reinterpret_cast<QRgb*> (tile.scanLine (y));
        for (int x = 0; x < size; ++x) {
            const int v = static_cast<int> (rng.bounded (256));
            const int a = static_cast<int> (rng.bounded (maxAlpha + 1));
            line[x] = qRgba (v, v, v, a);
        }
    }
    return tile;
}

inline void paintBeam (QPainter& p, const QPointF& center, qreal angle, qreal width, qreal length, const QColor& color, double alpha) {
    p.save();
    p.translate (center);
    p.rotate (angle);

    QColor edge = color;
    edge.setAlphaF (0.0);
    QColor peak = color;
    peak.setAlphaF (alpha);

    QLinearGradient gradient (-width / 2, 0, width / 2, 0);
    gradient.setColorAt (0.0, edge);
    gradient.setColorAt (0.5, peak);
    gradient.setColorAt (1.0, edge);
    p.fillRect (QRectF (-width / 2, -length / 2, width, length), gradient);

    QColor line = color;
    line.setAlphaF (alpha * 0.9);
    p.setPen (QPen (line, 1.0));
    p.drawLine (QPointF (width * 0.18, -length / 2), QPointF (width * 0.18, length / 2));
    p.restore();
}

inline void paintTexture (QPainter& p, const QSize& size, const Style& style) {
    const Theme::Colors& c = Theme::colors();
    const QRect all (QPoint (0, 0), size);
    const qreal w = size.width();
    const qreal h = size.height();
    const qreal length = qMax (w, h) * 3.0;

    p.fillRect (all, c.background);
    p.setRenderHint (QPainter::Antialiasing, true);

    if (style.beams) {
        paintBeam (p, QPointF (w * 0.16, h * 0.10), -33.0, w * 0.50, length, c.accent, style.beamAlpha);
        paintBeam (p, QPointF (w * 0.90, h * 0.95), -33.0, w * 0.42, length, c.aqua, style.beamAlpha * 0.55);
        paintBeam (p, QPointF (w * 0.62, h * 0.05), -33.0, w * 0.16, length, c.accent, style.beamAlpha * 0.45);
    }

    if (style.vignette) {
        QRadialGradient gradient (QPointF (w / 2, h / 2), qMax (w, h) * 0.78);
        gradient.setColorAt (0.0, QColor (0, 0, 0, 0));
        gradient.setColorAt (0.55, QColor (0, 0, 0, 0));
        gradient.setColorAt (1.0, QColor (0, 0, 0, qRound (255 * style.vignetteStrength)));
        p.fillRect (all, gradient);
    }

    if (style.dots && style.dotSpacing > 4) {
        const int step = style.dotSpacing;
        p.setPen (Qt::NoPen);
        p.setBrush (Theme::withAlpha (c.text, style.dotAlpha));
        int row = 0;
        for (int y = step / 2; y < size.height(); y += step, ++row) {
            int col = 0;
            for (int x = step / 2; x < size.width(); x += step, ++col) {
                if (row % 4 == 0 && col % 4 == 0)
                    continue;
                p.drawEllipse (QPointF (x, y), 1.0, 1.0);
            }
        }
        p.setPen (QPen (Theme::withAlpha (c.text, style.dotAlpha * 1.7), 1.0));
        row = 0;
        for (int y = step / 2; y < size.height(); y += step, ++row) {
            int col = 0;
            for (int x = step / 2; x < size.width(); x += step, ++col) {
                if (row % 4 == 0 && col % 4 == 0) {
                    p.drawLine (QPointF (x - 4, y), QPointF (x + 4, y));
                    p.drawLine (QPointF (x, y - 4), QPointF (x, y + 4));
                }
            }
        }
    }

    if (style.grain && style.grainAlpha > 0) {
        static QPixmap tile;
        static int tileAlpha = -1;
        if (tileAlpha != style.grainAlpha) {
            tile = QPixmap::fromImage (grainTile (192, style.grainAlpha));
            tileAlpha = style.grainAlpha;
        }
        p.setRenderHint (QPainter::SmoothPixmapTransform, false);
        p.fillRect (all, QBrush (tile));
    }
}

class Widget : public QWidget {
public:
    explicit Widget (QWidget* parent = nullptr)
        : QWidget (parent), m_style (currentStyle())
    {
        setAttribute (Qt::WA_OpaquePaintEvent);
    }

    void setStyle (const Style& style) {
        m_style = style;
        invalidate();
    }

    void invalidate() {
        m_cacheSize = QSize();
        update();
    }

    const QPixmap& pixmap() {
        ensureCache();
        return m_cache;
    }

    int generation() const { return m_generation; }

protected:
    void paintEvent (QPaintEvent* event) override {
        ensureCache();
        QPainter painter (this);
        painter.setClipRegion (event->region());
        painter.drawPixmap (QPoint (0, 0), m_cache);
    }

private:
    void ensureCache() {
        const qreal dpr = devicePixelRatioF();
        if (m_cacheSize == size() && qFuzzyCompare (m_cacheDpr, dpr))
            return;
        m_cacheSize = size();
        m_cacheDpr = dpr;

        m_cache = QPixmap (size() * dpr);
        m_cache.setDevicePixelRatio (dpr);
        QPainter painter (&m_cache);
        paintTexture (painter, size(), m_style);
        painter.end();
        ++m_generation;
    }

    Style m_style;
    QPixmap m_cache;
    QSize m_cacheSize;
    qreal m_cacheDpr = 0.0;
    int m_generation = 0;
};

}
