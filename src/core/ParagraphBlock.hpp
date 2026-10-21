#pragma once

#include "Block.hpp"

class ParagraphBlock : public Block {
private:
    QString m_text;

public:
    ParagraphBlock() = default;

    BlockType getType() const override {
        return BlockType::Paragraph;
    }

    void setContent (const QString& content) override {
        m_text = content;
    }

    QString getContent() const override {
        return m_text;
    }

    QString toMarkdown() const override {
        return m_text + "\n\n";
    }
};