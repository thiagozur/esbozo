#pragma once

#include "Block.hpp"

class EquationBlock : public Block {
private:
    QString m_content;

public:
    EquationBlock() = default;
    ~EquationBlock() override = default;

    BlockType getType() const override {
        return BlockType::Equation;
    }

    void setContent (const QString& content) override {
        m_content = content;
    }

    QString getContent() const override {
        return m_content;
    }
};