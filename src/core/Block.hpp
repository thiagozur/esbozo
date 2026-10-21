#pragma once

#include <QString>
#include <QPainter>
#include <QRectF>
#include <memory>

enum class BlockType {
    Paragraph,
    Title,
    Subtitle,
    Graph,
    Image
};

class Block {
public:
    virtual ~Block() = default;

    virtual BlockType getType() const = 0;

    virtual void render (QPainter& painter, const QRectF& rect) = 0;

    virtual qreal getHeight() const = 0;

    virtual QString toMarkdown() const = 0;

    virtual void setContent (const QString& content) = 0;
    virtual QString getContent() const = 0;
};