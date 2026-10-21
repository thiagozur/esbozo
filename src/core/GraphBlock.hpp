#pragma once

#include "Block.hpp"
#include <QVector>
#include <QPointF>

class GraphBlock : public Block {
private:
    QString m_title;
    QVector<QPointF> m_dataPoints;
    QString m_csvFilePath;

public:
    GraphBlock (const QString& title) : m_title (title) {}

    BlockType getType() const override {
        return BlockType::Graph;
    }

    void setContent (const QString& csvPathOrData) override {
        m_csvFilePath = csvPathOrData;
        // parsear data o csv
    }

    QString getContent() const override {
        return m_csvFilePath;
    }

    void addDataPoint(qreal x, qreal y) {
        m_dataPoints.append(QPointF(x, y));
    }

    qreal getHeight() const override {
        return 200.0;
    }

    void render(QPainter& painter, const QRectF& rect) override {
        painter.save();
        
        painter.setPen (Qt::gray);
        painter.setBrush (QColor(245, 245, 245));
        painter.drawRoundedRect (rect, 6, 6);

        painter.setPen (Qt::black);
        painter.setFont (QFont("Segoe UI", 10, QFont::Bold));
        painter.drawText (rect.adjusted(10, 10, -10, 0), Qt::AlignLeft | Qt::AlignTop, m_title);
        
        painter.restore();
    }

    QString toMarkdown() const override {
        return QString("[Gráfico: %1 (Datos: %2)]\n\n").arg(m_title, m_csvFilePath);
    }
};