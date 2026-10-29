#include "io/ProjectFile.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

#include "core/EquationBlock.hpp"
#include "core/GraphBlock.hpp"
#include "core/ImageBlock.hpp"
#include "core/ParagraphBlock.hpp"
#include "core/SubtitleBlock.hpp"
#include "core/TableBlock.hpp"
#include "core/TitleBlock.hpp"

namespace {

const char* typeName (BlockType type) {
    switch (type) {
        case BlockType::Title: return "Title";
        case BlockType::Subtitle: return "Subtitle";
        case BlockType::Paragraph: return "Paragraph";
        case BlockType::Image: return "Image";
        case BlockType::Graph: return "Graph";
        case BlockType::Equation: return "Equation";
        case BlockType::Table: return "Table";
    }
    return "";
}

QJsonArray toJson (const QVector<double>& values) {
    QJsonArray arr;
    for (double v : values)
        arr.append (v);
    return arr;
}

QVector<double> toDoubles (const QJsonArray& arr) {
    QVector<double> values;
    for (const QJsonValue& v : arr)
        values.append (v.toDouble());
    return values;
}

QJsonObject graphToJson (const GraphBlock& g) {
    QJsonObject obj;
    obj["graphType"] = (g.getGraphType() == GraphType::Line) ? "Line" : "Scatter";
    obj["graphSource"] = (g.getGraphSource() == GraphSource::Csv) ? "Csv" : "Manual";
    obj["scaleX"] = (g.getScaleX() == AxisScale::Log) ? "Log" : "Linear";
    obj["scaleY"] = (g.getScaleY() == AxisScale::Log) ? "Log" : "Linear";
    obj["description"] = g.getDescription();
    obj["csvPath"] = g.getCsvPath();

    QJsonObject xObj;
    xObj["name"] = g.getSeriesX().name;
    xObj["values"] = toJson (g.getSeriesX().values);
    obj["seriesX"] = xObj;

    QJsonArray ySeries;
    for (const DataSeries& ds : g.getSeriesYList()) {
        QJsonObject dsObj;
        dsObj["name"] = ds.name;
        dsObj["values"] = toJson (ds.values);
        ySeries.append (dsObj);
    }
    obj["seriesYList"] = ySeries;
    return obj;
}

QJsonObject tableToJson (const TableBlock& t) {
    QJsonObject obj;
    obj["csvPath"] = t.getCsvPath();
    obj["hasHeaderRow"] = t.hasHeaderRow();
    obj["hasRowNames"] = t.hasRowNames();

    QJsonArray rows;
    for (const QStringList& row : t.getData()) {
        QJsonArray r;
        for (const QString& cell : row)
            r.append (cell);
        rows.append (r);
    }
    obj["data"] = rows;
    return obj;
}

std::unique_ptr<Block> graphFromJson (const QJsonObject& obj, const QString& content) {
    auto b = std::make_unique<GraphBlock>();
    b->setContent (content);
    b->setGraphType (obj["graphType"].toString() == "Scatter" ? GraphType::Scatter : GraphType::Line);
    b->setGraphSource (obj["graphSource"].toString() == "Csv" ? GraphSource::Csv : GraphSource::Manual);
    b->setScales (obj["scaleX"].toString() == "Log" ? AxisScale::Log : AxisScale::Linear, obj["scaleY"].toString() == "Log" ? AxisScale::Log : AxisScale::Linear);
    b->setDescription (obj["description"].toString());
    b->setCsvPath (obj["csvPath"].toString());

    const QJsonObject xObj = obj["seriesX"].toObject();
    b->setSeriesX (xObj["name"].toString(), toDoubles (xObj["values"].toArray()));

    QVector<DataSeries> ySeriesList;
    for (const QJsonValue& yv : obj["seriesYList"].toArray()) {
        const QJsonObject dsObj = yv.toObject();
        DataSeries ds;
        ds.name = dsObj["name"].toString();
        ds.values = toDoubles (dsObj["values"].toArray());
        ySeriesList.append (ds);
    }
    b->setSeriesYList (ySeriesList);
    return b;
}

std::unique_ptr<Block> tableFromJson (const QJsonObject& obj, const QString& content) {
    auto b = std::make_unique<TableBlock>();
    b->setContent (content);
    b->setCsvPath (obj["csvPath"].toString());
    b->setHasHeaderRow (obj["hasHeaderRow"].toBool (true));
    b->setHasRowNames (obj["hasRowNames"].toBool (false));

    QVector<QStringList> data;
    for (const QJsonValue& rv : obj["data"].toArray()) {
        QStringList row;
        for (const QJsonValue& cv : rv.toArray())
            row << cv.toString();
        data.append (row);
    }
    b->setData (data);
    return b;
}

std::unique_ptr<Block> textBlock (const QString& typeStr, const QString& content) {
    std::unique_ptr<Block> b;
    if (typeStr == "Title") b = std::make_unique<TitleBlock>();
    else if (typeStr == "Subtitle") b = std::make_unique<SubtitleBlock>();
    else if (typeStr == "Paragraph") b = std::make_unique<ParagraphBlock>();
    else if (typeStr == "Image") b = std::make_unique<ImageBlock>();
    else if (typeStr == "Equation") b = std::make_unique<EquationBlock>();
    if (b)
        b->setContent (content);
    return b;
}

void setError (QString* error, const QString& message) {
    if (error)
        *error = message;
}

}

namespace ProjectFile {

bool save (const Document& doc, const QString& path, QString* error) {
    QJsonObject root;
    root["sbz_version"] = "1.0";

    QJsonArray blocks;
    for (size_t i = 0; i < doc.getBlockCount(); ++i) {
        const Block* block = doc.getBlock (i);
        if (!block)
            continue;

        QJsonObject obj;
        obj["type"] = typeName (block->getType());
        obj["content"] = block->getContent();

        if (const GraphBlock* g = dynamic_cast<const GraphBlock*> (block)) {
            const QJsonObject extra = graphToJson (*g);
            for (auto it = extra.begin(); it != extra.end(); ++it)
                obj[it.key()] = it.value();
        } else if (const TableBlock* t = dynamic_cast<const TableBlock*> (block)) {
            const QJsonObject extra = tableToJson (*t);
            for (auto it = extra.begin(); it != extra.end(); ++it)
                obj[it.key()] = it.value();
        }
        blocks.append (obj);
    }
    root["blocks"] = blocks;

    QFile file (path);
    if (!file.open (QIODevice::WriteOnly)) {
        setError (error, "Error al guardar el archivo");
        return false;
    }
    file.write (QJsonDocument (root).toJson());
    file.close();
    return true;
}

bool load (Document& doc, const QString& path, QString* error) {
    QFile file (path);
    if (!file.open (QIODevice::ReadOnly)) {
        setError (error, "Error al abrir el archivo");
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument json = QJsonDocument::fromJson (data);
    if (json.isNull() || !json.isObject()) {
        setError (error, "El archivo no es válido");
        return false;
    }

    Document loaded;
    for (const QJsonValue& val : json.object()["blocks"].toArray()) {
        const QJsonObject obj = val.toObject();
        const QString typeStr = obj["type"].toString();
        const QString content = obj["content"].toString();

        std::unique_ptr<Block> block;
        if (typeStr == "Graph") block = graphFromJson (obj, content);
        else if (typeStr == "Table") block = tableFromJson (obj, content);
        else block = textBlock (typeStr, content);

        if (block)
            loaded.addBlock (std::move (block));
    }

    if (loaded.getBlockCount() == 0)
        loaded.addBlock (std::make_unique<TitleBlock>());

    doc = std::move (loaded);
    return true;
}

}