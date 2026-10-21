#pragma once

#include "Block.hpp"
#include <QString>
#include <QVector>
#include <QPair>

enum class GraphType {
    Line,
    Scatter
};

enum class GraphSource {
    Csv,
    Manual
};

enum class AxisScale {
    Linear,
    Log
};

struct DataSeries {
    QString name;
    QVector<double> values;
};

class GraphBlock : public Block {
private:
    GraphType m_graphType = GraphType::Line;
    GraphSource m_graphSource = GraphSource::Manual;
    DataSeries m_seriesX;
    QVector<DataSeries> m_seriesYList;
    AxisScale m_scaleX = AxisScale::Linear;
    AxisScale m_scaleY = AxisScale::Linear;
    QString m_description;
    QString m_csvPath;

public:
    explicit GraphBlock (const QString& title = "") : m_description(title) {}

    BlockType getType() const override { return BlockType::Graph; }

    QString toMarkdown() const override { return ""; } // se implementa despues

    void setGraphType (GraphType type) { m_graphType = type; }
    void setGraphSource (GraphSource source) { m_graphSource = source; }
    void setSeriesX (const QString& name, const QVector<double>& vals) { m_seriesX = {name, vals}; }
    void setSeriesY (const QString& name, const QVector<double>& vals) { m_seriesYList = { DataSeries{name, vals} }; }
    void setSeriesYList (const QVector<DataSeries>& list) { m_seriesYList = list; }
    void setScales (AxisScale x, AxisScale y) { m_scaleX = x; m_scaleY = y; }
    void setDescription (const QString& desc) { m_description = desc; }
    void setCsvPath (const QString& path) { m_csvPath = path; }

    GraphType getGraphType() const { return m_graphType; }
    GraphSource getGraphSource() const { return m_graphSource; }
    const DataSeries& getSeriesX() const { return m_seriesX; }
    const QVector<DataSeries>& getSeriesYList() const { return m_seriesYList; }
    AxisScale getScaleX() const { return m_scaleX; }
    AxisScale getScaleY() const { return m_scaleY; }
    QString getDescription() const { return m_description; }
    QString getCsvPath() const { return m_csvPath; }

    QString getContent() const override { return m_description; }
    void setContent (const QString& content) override { m_description = content; }
};