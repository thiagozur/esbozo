#pragma once

#include "Document.hpp"
#include "TitleBlock.hpp"
#include "SubtitleBlock.hpp"
#include "ParagraphBlock.hpp"
#include "ImageBlock.hpp"
#include "GraphBlock.hpp"
#include "EquationBlock.hpp"
#include <QString>
#include <memory>

enum class CommandResult {
    None,
    BlockCreated,
    BlockReset,
    BlockDeleted,
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
            m_document.addBlock (std::make_unique<TitleBlock>());
        }
    }

    static QStringList getSupportedCommands() {
        return {"cpar;;", "cttl;;", "csttl;;", "rst;;", "del;;", "sttl;;", "ttl;;", "par;;", "grf;;", "ft;;", "eq;;"};
    }

    CommandResult processInput (const QString& input) {
        if (input.isEmpty()) return CommandResult::None;

        QString textPayload = input;
        QString triggeredCommand = "";

        QStringList possibleCmds = getSupportedCommands();
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
            createNewBlock (std::make_unique<TitleBlock>());
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "sttl;;") {
            createNewBlock (std::make_unique<SubtitleBlock>());
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "par;;") {
            createNewBlock (std::make_unique<ParagraphBlock>());
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "grf;;") {
            createNewBlock (std::make_unique<GraphBlock> ("Gráfico"));
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "ft;;") {
            createNewBlock(std::make_unique<ImageBlock>());
            return CommandResult::BlockCreated;
        } else if (triggeredCommand == "eq;;") {
            createNewBlock (std::make_unique<EquationBlock>());
            return CommandResult::BlockCreated;
        }
        else if (triggeredCommand == "del;;") {
            if (m_document.getBlockCount() > 1) {
                m_document.removeBlock (m_activeBlockIndex);
                if (m_activeBlockIndex > 0) {
                    m_activeBlockIndex--;
                }
            } else {
                if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                    current->setContent ("");
                }
            }
            return CommandResult::BlockDeleted;
        }
        else if (triggeredCommand == "cpar;;") {
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                auto newBlock = std::make_unique<ParagraphBlock>();
                newBlock->setContent (current->getContent());
                m_document.setBlock (m_activeBlockIndex, std::move (newBlock));
            }
            return CommandResult::TextUpdated;
        }
        else if (triggeredCommand == "cttl;;") {
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                auto newBlock = std::make_unique<TitleBlock>();
                newBlock->setContent (current->getContent());
                m_document.setBlock (m_activeBlockIndex, std::move (newBlock));
            }
            return CommandResult::TextUpdated;
        }
        else if (triggeredCommand == "csttl;;") {
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                auto newBlock = std::make_unique<SubtitleBlock>();
                newBlock->setContent (current->getContent());
                m_document.setBlock (m_activeBlockIndex, std::move (newBlock));
            }
            return CommandResult::TextUpdated;
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