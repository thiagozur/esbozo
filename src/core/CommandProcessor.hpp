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
        if (input.isEmpty()) return CommandResult::None;

        QString textPayload = input;
        QString triggeredCommand = "";

        QStringList possibleCmds = {"rst;;", "ttl;;", "sttl;;", "par;;", "grf;;", "ft;;"};
        for (const QString& cmd : possibleCmds) {
            if (textPayload.trimmed().endsWith (cmd)) {
                triggeredCommand = cmd;
                int index = textPayload.lastIndexOf (cmd);
                textPayload = textPayload.left (index).trimmed();
                break;
            }
        }

        if (!triggeredCommand.isEmpty() || !textPayload.isEmpty()) {
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                current->setContent (textPayload);
            }
            if (triggeredCommand.isEmpty()) {
                return CommandResult::TextUpdated;
            }
        }

        auto createNewBlock = [&] (std::unique_ptr<Block> newBlock) {
            size_t insertIndex = m_activeBlockIndex + 1;

            if (m_document.getBlockCount() == 1) {
                if (Block* current = m_document.getBlock (0)) {
                    if (current->getContent().isEmpty()) {
                        m_document.clear();
                        insertIndex = 0;
                    }
                }
            }

            m_document.insertBlock (insertIndex, std::move (newBlock));
            m_activeBlockIndex = insertIndex;
        };

        if (triggeredCommand == "rst;;") {
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                current->setContent ("");
            }
            return CommandResult::BlockReset;
        }
        else if (triggeredCommand == "ttl;;") {
            createNewBlock(std::make_unique<ParagraphBlock> (true));
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "sttl;;") {
            auto block = std::make_unique<ParagraphBlock> (true);
            block->setContent ("Subtitle");
            createNewBlock(std::move (block));
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "par;;") {
            createNewBlock(std::make_unique<ParagraphBlock> (false));
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "grf;;") {
            createNewBlock(std::make_unique<GraphBlock> ("Gráfico"));
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "ft;;") {
            auto block = std::make_unique<ParagraphBlock> (false);
            block->setContent ("[Figura/Imagen]");
            createNewBlock(std::move (block));
            return CommandResult::BlockCreated;
        }

        return CommandResult::None;
    }

    size_t getActiveBlockIndex() const {
        return m_activeBlockIndex;
    }

    void setActiveBlockIndex (size_t index) {
        if (index < m_document.getBlockCount()) {
            m_activeBlockIndex = index;
        }
    }
};