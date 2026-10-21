#pragma once

#include "Block.hpp"

class TitleBlock : public Block {
private:
    QString m_text;

public:
    TitleBlock() = default;

    BlockType getType() const override {
        return BlockType::Title;
    }

    void setContent (const QString& content) override {
        m_text = content;
    }

    QString getContent() const override {
        return m_text;
    }

    QString toMarkdown() const override {
        return "# " + m_text + "\n\n";
    }
};