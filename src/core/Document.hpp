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

    void insertBlock (size_t index, std::unique_ptr<Block> block) {
        if (index > m_blocks.size()) index = m_blocks.size();
        m_blocks.insert (m_blocks.begin() + static_cast<std::ptrdiff_t> (index), std::move (block));
    }

    void setBlock (size_t index, std::unique_ptr<Block> block) {
        if (index < m_blocks.size())
            m_blocks[index] = std::move (block);
    }
    
    void removeBlock (size_t index) {
        if (index < m_blocks.size()) {
            m_blocks.erase (m_blocks.begin() + static_cast<std::ptrdiff_t> (index));
        }
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

    void clear() {
        m_blocks.clear();
    }
};