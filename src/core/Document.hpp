#pragma once

#include "Block.hpp"
#include <vector>
#include <memory>
#include <QString>

class Document {
private:
    std::vector<std::unique_ptr<Block>> m_blocks;

public:
    Document() = default;

    void addBlock (std::unique_ptr<Block> block) {
        m_blocks.push_back(std::move(block));
    }

    size_t getBlockCount() const {
        return m_blocks.size();
    }

    Block* getBlock (size_t index) const {
        if (index < m_blocks.size()) {
            return m_blocks[index].get();
        }
        return nullptr;
    }

    QString toMarkdown() const {
        QString fullMarkdown;
        for (const auto& block : m_blocks) {
            fullMarkdown += block->toMarkdown();
        }
        return fullMarkdown;
    }

    void clear() {
        m_blocks.clear();
    }
};