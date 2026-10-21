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
        if (trimmed.isEmpty()) return CommandResult::None;

        auto checkTrailingCommand = [&] (const QString& cmdKeyword) -> bool {
            return trimmed.endsWith (cmdKeyword) && trimmed != cmdKeyword;
        };

        QString textPayload = "";
        QString triggeredCommand = "";

        if (trimmed == "res;;") {
            triggeredCommand = "res;;";
        } else if (trimmed == "sg;;") {
            triggeredCommand = "sg;;";
        } else if (trimmed == "ttl;;") {
            triggeredCommand = "ttl;;";
        } else if (trimmed == "sttl;;") {
            triggeredCommand = "sttl;;";
        } else if (trimmed == "par;;") {
            triggeredCommand = "par;;";
        } else if (trimmed == "grf;;") {
            triggeredCommand = "grf;;";
        } else if (trimmed == "ft;;") {
            triggeredCommand = "ft;;";
        } 
        else {
            QStringList possibleCmds = {"res;;", "sg;;", "ttl;;", "sttl;;", "par;;", "grf;;", "ft;;"};
            for (const QString& cmd : possibleCmds) {
                if (trimmed.endsWith (cmd)) {
                    triggeredCommand = cmd;
                    textPayload = trimmed.left (trimmed.length() - cmd.length()).trimmed();
                    break;
                }
            }
        }

        if (!textPayload.isEmpty() || triggeredCommand.isEmpty()) {
            QString contentToAdd = textPayload.isEmpty() ? input : textPayload;
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                QString existing = current->getContent();
                if (existing.isEmpty()) {
                    current->setContent (contentToAdd);
                } else {
                    current->setContent (existing + " " + contentToAdd);
                }
            }
            if (triggeredCommand.isEmpty()) {
                return CommandResult::TextUpdated;
            }
        }

        auto createNewBlock = [&] (std::unique_ptr<Block> newBlock) {
            if (m_document.getBlockCount() == 1) {
                if (Block* current = m_document.getBlock (0)) {
                    if (current->getContent().isEmpty()) {
                        m_document.clear();
                    }
                }
            }
            m_document.addBlock (std::move (newBlock));
            m_activeBlockIndex = m_document.getBlockCount() - 1;
        };

        if (triggeredCommand == "res;;") {
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                current->setContent ("");
            }
            return CommandResult::BlockReset;
        }
        else if (triggeredCommand == "sg;;") {
            createNewBlock(std::make_unique<ParagraphBlock> (false));
            return CommandResult::SectionFinished;
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
};