#pragma once

#include "Block.hpp"

class ParagraphBlock : public Block {
private:
    QString m_text;
    bool m_isTitle;

public:
    ParagraphBlock (bool isTitle = false) : m_isTitle (isTitle) {}

    BlockType getType() const override {
        return m_isTitle ? BlockType::Title : BlockType::Paragraph;
    }

    void setContent (const QString& content) override {
        m_text = content;
    }

    QString getContent() const override {
        return m_text;
    }

    qreal getHeight() const override {
        return m_isTitle ? 40.0 : 24.0;
    }

    void render (QPainter& painter, const QRectF& rect) override {
        painter.save();
        if (m_isTitle) {
            painter.setFont(QFont ("Segoe UI", 16, QFont::Bold));
        } else {
            painter.setFont(QFont ("Segoe UI", 11, QFont::Normal));
        }
        painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, m_text);
        painter.restore();
    }

    QString toMarkdown() const override {
        if (m_isTitle) {
            return "# " + m_text + "\n\n";
        }
        return m_text + "\n\n";
    }
};