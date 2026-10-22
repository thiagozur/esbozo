#pragma once

#include "Block.hpp"

class ImageBlock : public Block {
private:
    QString m_imagePath;

public:
    ImageBlock() = default;

    BlockType getType() const override {
        return BlockType::Image;
    }

    void setContent (const QString& content) override {
        m_imagePath = content;
    }

    QString getContent() const override {
        return m_imagePath;
    }
};