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

    QString toMarkdown() const override {
        return QString("[Gráfico: %1 (Datos: %2)]\n\n").arg(m_title, m_csvFilePath);
    }
};