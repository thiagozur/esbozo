#pragma once

#include "Block.hpp"

class EquationBlock : public Block {
public:
    EquationBlock() = default;

    explicit EquationBlock (const QString& latex) : m_content (latex) {}

    BlockType getType() const override {
        return BlockType::Equation;
    }

    void setContent (const QString& c) override {
        m_content = c;
    }

    QString getContent() const override {
        return m_content;
    }

private:
    QString m_content;
};