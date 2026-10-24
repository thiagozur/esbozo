#pragma once

#include <QString>
#include <QVector>
#include <QStringList>

#include "Block.hpp"

class TableBlock : public Block {
private:
    QString m_tableTitle;
    QString m_csvPath;

    bool m_hasHeaderRow = true;
    bool m_hasRowNames = false;

    QVector<QStringList> m_data;

public:
   explicit TableBlock (const QString& title = "") : m_tableTitle (title) {}
    ~TableBlock() override = default;

    BlockType getType() const override { return BlockType::Table; }

    void setTitle (const QString& title) { m_tableTitle = title; }
    void setCsvPath (const QString& path) { m_csvPath = path; }
    void setHasHeaderRow (bool hasHeader) { m_hasHeaderRow = hasHeader; }
    void setHasRowNames (bool hasRowNames) { m_hasRowNames = hasRowNames; }
    void setData (const QVector<QStringList>& data) { m_data = data; }

    QString getTitle() const { return m_tableTitle; }
    QString getCsvPath() const { return m_csvPath; }
    bool hasHeaderRow() const { return m_hasHeaderRow; }
    bool hasRowNames() const { return m_hasRowNames; }
    const QVector<QStringList>& getData() const { return m_data; }

    QString getContent() const override { return m_tableTitle; }
    void setContent (const QString& content) override { m_tableTitle = content; }
};