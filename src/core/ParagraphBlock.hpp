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

    QString toMarkdown() const override {
        if (m_isTitle) {
            return "# " + m_text + "\n\n";
        }
        return m_text + "\n\n";
    }
};