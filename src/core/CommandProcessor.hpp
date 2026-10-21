#pragma once

#include "Document.hpp"
#include "ParagraphBlock.hpp"
#include "GraphBlock.hpp"
#include <QString>
#include <memory>

enum class CommandResult {
    None,
    BlockCreated,
    BlockReset,
    SectionFinished,
    TextUpdated
};

class CommandProcessor {
private:
    Document& m_document;
    size_t m_activeBlockIndex;

public:
    CommandProcessor (Document& doc) : m_document (doc), m_activeBlockIndex (0) {
        if (m_document.getBlockCount() == 0) {
            m_document.addBlock (std::make_unique<ParagraphBlock> (false));
        }
    }

    CommandResult processInput (const QString& input) {
        QString trimmed = input.trimmed();

        if (trimmed == "res;;") {
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                current->setContent ("");
            }
            return CommandResult::BlockReset;
        }

        if (trimmed == "sg;;") {
            m_document.addBlock (std::make_unique<ParagraphBlock> (false));
            m_activeBlockIndex = m_document.getBlockCount() - 1;
            return CommandResult::SectionFinished;
        }

        if (trimmed == "ttl;;") {
            m_document.addBlock (std::make_unique<ParagraphBlock> (true));
            m_activeBlockIndex = m_document.getBlockCount() - 1;
            return CommandResult::BlockCreated;
        }
        else if (trimmed == "sttl;;") {
            auto block = std::make_unique<ParagraphBlock> (true);
            block->setContent ("Subtitle");
            m_document.addBlock (std::move (block));
            m_activeBlockIndex = m_document.getBlockCount() - 1;
            return CommandResult::BlockCreated;
        }
        else if (trimmed == "par;;") {
            m_document.addBlock (std::make_unique<ParagraphBlock> (false));
            m_activeBlockIndex = m_document.getBlockCount() - 1;
            return CommandResult::BlockCreated;
        }
        else if (trimmed == "grf;;") {
            m_document.addBlock (std::make_unique<GraphBlock> ("Gráfico"));
            m_activeBlockIndex = m_document.getBlockCount() - 1;
            return CommandResult::BlockCreated;
        }
        else if (trimmed == "ft;;") {
            auto block = std::make_unique<ParagraphBlock> (false);
            block->setContent ("[Figura/Imagen]");
            m_document.addBlock (std::move (block));
            m_activeBlockIndex = m_document.getBlockCount() - 1;
            return CommandResult::BlockCreated;
        }

        if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
            current->setContent (input);
        }
        return CommandResult::TextUpdated;
    }
};