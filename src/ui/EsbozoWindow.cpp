#include "ui/EsbozoWindow.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

#include "core/EquationBlock.hpp"
#include "core/GraphBlock.hpp"
#include "core/InlineMarkup.hpp"
#include "core/PdfExporter.hpp"
#include "core/TableBlock.hpp"
#include "core/TitleBlock.hpp"
#include "io/MarkdownExporter.hpp"
#include "io/ProjectFile.hpp"
#include "ui/BlockWidgets.hpp"
#include "ui/Dialogs.hpp"
#include "ui/Theme.hpp"

EsbozoWindow::EsbozoWindow()
    : m_processor (m_document), m_activeBlockIndex (0), m_wizard (*this)
{
    updateWindowTitle();
    resize (Theme::metrics().windowWidth, Theme::metrics().windowHeight);

    if (m_document.getBlockCount() == 0)
        m_document.addBlock (std::make_unique<TitleBlock>());

    QWidget* centralWidget = new QWidget (this);
    QVBoxLayout* mainLayout = new QVBoxLayout (centralWidget);

    m_scrollArea = new QScrollArea (this);
    m_scrollArea->setWidgetResizable (true);

    m_containerWidget = new QWidget();
    m_documentLayout = new QVBoxLayout (m_containerWidget);
    m_documentLayout->setAlignment (Qt::AlignTop);
    m_documentLayout->setSpacing (Theme::metrics().blockSpacing);
    m_scrollArea->setWidget (m_containerWidget);

    mainLayout->addWidget (m_scrollArea);

    m_statusLabel = new QLabel ("[Ctrl + Up / ,] subir, [Ctrl + Down / .] bajar. Enter procesa comandos.", this);
    m_statusLabel->setStyleSheet (Theme::statusStyle());
    mainLayout->addWidget (m_statusLabel);

    setCentralWidget (centralWidget);

    refreshUI();
}

bool EsbozoWindow::eventFilter (QObject* obj, QEvent* event) {
    if (obj == m_activeTextEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*> (event);

        if (keyEvent->key() == Qt::Key_Tab && keyEvent->modifiers() == Qt::NoModifier) {
            m_activeTextEdit->moveCursor (QTextCursor::End);
            return true;
        }

        if (keyEvent->modifiers() == Qt::ControlModifier) {
            if (keyEvent->key() == Qt::Key_Comma || keyEvent->key() == Qt::Key_Up) {
                saveCurrentContent();
                saveAndNavigate (-1);
                return true;
            }
            else if (keyEvent->key() == Qt::Key_Period || keyEvent->key() == Qt::Key_Down) {
                saveCurrentContent();
                saveAndNavigate (1);
                return true;
            }
            else if (keyEvent->key() == Qt::Key_S) {
                saveCurrentContent();
                saveProject();
                return true;
            }
            else if (keyEvent->key() == Qt::Key_O) {
                openProject();
                return true;
            }
            else if (keyEvent->key() == Qt::Key_N) {
                newProject();
                return true;
            }
            else if (keyEvent->key() == Qt::Key_E) {
                startExportWizard();
                return true;
            }
            else if (keyEvent->key() == Qt::Key_B) {
                toggleBold();
                return true;
            }
            else if (keyEvent->key() == Qt::Key_K) {
                toggleItalic();
                return true;
            }
        }

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (m_wizard.active()) {
                handleInputSubmitted();
                return true;
            }

            const QString trimmedText = m_activeTextEdit->toPlainText().trimmed();
            for (const QString& cmd : CommandProcessor::getSupportedCommands()) {
                if (trimmedText.endsWith (cmd)) {
                    handleInputSubmitted();
                    return true;
                }
            }
        }
    }
    return QMainWindow::eventFilter (obj, event);
}

void EsbozoWindow::keyPressEvent (QKeyEvent* event) {
    if (event->modifiers() == Qt::ControlModifier) {
        if (event->key() == Qt::Key_Comma || event->key() == Qt::Key_Up) {
            saveCurrentContent();
            saveAndNavigate (-1);
            event->accept();
            return;
        }
        else if (event->key() == Qt::Key_Period || event->key() == Qt::Key_Down) {
            saveCurrentContent();
            saveAndNavigate (1);
            event->accept();
            return;
        }
        else if (event->key() == Qt::Key_S) {
            saveCurrentContent();
            saveProject();
            event->accept();
            return;
        }
        else if (event->key() == Qt::Key_O) {
            openProject();
            event->accept();
            return;
        }
        else if (event->key() == Qt::Key_N) {
            newProject();
            event->accept();
            return;
        }
        else if (event->key() == Qt::Key_E) {
            startExportWizard();
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent (event);
}

void EsbozoWindow::resizeEvent (QResizeEvent* event) {
    QMainWindow::resizeEvent (event);
    updateImageWidths();
    centerActiveBlock();
}

void EsbozoWindow::setPrompt (const QString& text) {
    if (!m_activeTextEdit)
        return;
    m_activeTextEdit->clear();
    m_activeTextEdit->setPlaceholderText (text);
    m_activeTextEdit->setStyleSheet (Theme::wizardStyle());
}

void EsbozoWindow::clearInput() {
    if (m_activeTextEdit)
        m_activeTextEdit->clear();
}

void EsbozoWindow::showMessage (const QString& text) {
    m_statusLabel->setText (text);
}

QString EsbozoWindow::pickFile (const QString& title, const QString& nameFilter) {
    return Dialogs::pickFile (this, title, nameFilter);
}

void EsbozoWindow::exportAs (const QString& format) {
    if (format == "md")
        exportToMarkdown();
    else if (format == "pdf")
        exportToPdf();
}

void EsbozoWindow::discardActiveBlock() {
    if (m_document.getBlockCount() > 1) {
        size_t indexToDelete = m_activeBlockIndex;
        if (m_activeBlockIndex > 0)
            m_activeBlockIndex--;
        m_document.removeBlock (indexToDelete);
    } else if (Block* b = m_document.getBlock (m_activeBlockIndex)) {
        b->setContent ("");
    }
    refreshUI();
}

void EsbozoWindow::newProject() {
    saveCurrentContent();
    m_document = Document();
    m_document.addBlock (std::make_unique<TitleBlock>());
    m_activeBlockIndex = 0;
    m_currentFilePath.clear();
    m_wizard.reset();
    updateWindowTitle();
    m_statusLabel->setText ("Nuevo proyecto creado");
    refreshUI();
}

void EsbozoWindow::saveProject() {
    if (m_currentFilePath.isEmpty()) {
        QString filePath = QFileDialog::getSaveFileName (this, "Guardar Proyecto", "", "Archivos Esbozo (*.sbz)");
        if (filePath.isEmpty())
            return;
        if (!filePath.endsWith (".sbz", Qt::CaseInsensitive))
            filePath += ".sbz";
        m_currentFilePath = filePath;
    }

    QString error;
    if (ProjectFile::save (m_document, m_currentFilePath, &error)) {
        updateWindowTitle();
        m_statusLabel->setText ("Proyecto guardado exitosamente en: " + m_currentFilePath);
    } else {
        m_statusLabel->setText (error);
    }
}

void EsbozoWindow::openProject() {
    const QString filePath = QFileDialog::getOpenFileName (this, "Abrir Proyecto", "", "Archivos Esbozo (*.sbz)");
    if (!filePath.isEmpty())
        loadProject (filePath);
}

bool EsbozoWindow::loadProject (const QString& path) {
    QString error;
    if (!ProjectFile::load (m_document, path, &error)) {
        m_statusLabel->setText (error);
        return false;
    }

    m_activeBlockIndex = 0;
    m_currentFilePath = path;
    m_wizard.reset();
    updateWindowTitle();
    m_statusLabel->setText ("Proyecto cargado exitosamente desde: " + path);
    refreshUI();
    return true;
}

void EsbozoWindow::updateWindowTitle() {
    QString title = "Esbozo";
    if (!m_currentFilePath.isEmpty())
        title += " - " + QFileInfo (m_currentFilePath).fileName();
    else
        title += " - [Nuevo Proyecto]";
    setWindowTitle (title);
}

void EsbozoWindow::startExportWizard() {
    if (navigationLocked()) {
        showLockedMessage();
        return;
    }
    saveCurrentContent();
    m_wizard.startExport();
}

void EsbozoWindow::exportToMarkdown() {
    QString filePath = QFileDialog::getSaveFileName (this, "Exportar a Markdown", "", "Archivos Markdown (*.md)");
    if (filePath.isEmpty())
        return;
    if (!filePath.endsWith (".md", Qt::CaseInsensitive))
        filePath += ".md";

    if (MarkdownExporter::exportTo (m_document, filePath))
        m_statusLabel->setText ("Documento exportado exitosamente a: " + filePath);
    else
        m_statusLabel->setText ("Error al exportar el archivo Markdown");
}

void EsbozoWindow::exportToPdf() {
    QString filePath = QFileDialog::getSaveFileName (this, "Exportar a PDF", "", "Archivos PDF (*.pdf)");
    if (filePath.isEmpty())
        return;
    if (!filePath.endsWith (".pdf", Qt::CaseInsensitive))
        filePath += ".pdf";

    QApplication::setOverrideCursor (Qt::WaitCursor);
    PdfExporter::Style style;
    style.boldAsOutlines = false;
    PdfExporter::Result res = PdfExporter::exportDocument (m_document, filePath, style);
    QApplication::restoreOverrideCursor();

    if (!res.ok)
        m_statusLabel->setText ("Error al exportar el PDF: " + res.message);
    else if (!res.message.isEmpty())
        m_statusLabel->setText ("PDF exportado a: " + filePath + " (" + res.message + ")");
    else
        m_statusLabel->setText ("Documento exportado exitosamente a: " + filePath);
}

void EsbozoWindow::handleInputSubmitted() {
    if (!m_activeTextEdit)
        return;

    if (m_wizard.active()) {
        m_wizard.submit (m_activeTextEdit->toPlainText());
        return;
    }

    m_processor.setActiveBlockIndex (m_activeBlockIndex);
    CommandResult result = m_processor.processInput (inputForProcessor());

    if (result == CommandResult::BlockCreated || result == CommandResult::SectionFinished || result == CommandResult::BlockDeleted)
        m_activeBlockIndex = m_processor.getActiveBlockIndex();

    refreshUI();

    if (result == CommandResult::BlockCreated || result == CommandResult::BlockReset)
        m_wizard.beginForActiveBlock();
}

bool EsbozoWindow::navigationLocked() const {
    return m_wizard.active();
}

void EsbozoWindow::showLockedMessage() {
    const QString previous = m_statusLabel->text();
    m_statusLabel->setText ("Terminá la operación actual o cancelala con 'can;;' antes de cambiar de bloque");
    QTimer::singleShot (2500, this, [this, previous]() {
        m_statusLabel->setText (previous);
    });
}

void EsbozoWindow::activateBlock (size_t index) {
    if (navigationLocked()) {
        showLockedMessage();
        return;
    }
    saveCurrentContent();
    m_activeBlockIndex = index;
    refreshUI();
}

void EsbozoWindow::saveCurrentContent() {
    if (navigationLocked())
        return;
    if (m_activeTextEdit && m_activeBlockIndex < m_document.getBlockCount()) {
        if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
            current->setContent (InlineMarkup::isRichType (current->getType())
                ? InlineMarkup::serialize (m_activeTextEdit->document())
                : m_activeTextEdit->toPlainText());
        }
    }
}

void EsbozoWindow::saveAndNavigate (int delta) {
    if (navigationLocked()) {
        showLockedMessage();
        return;
    }
    saveCurrentContent();

    long long newIndex = static_cast<long long> (m_activeBlockIndex) + delta;
    if (newIndex >= 0 && newIndex < static_cast<long long> (m_document.getBlockCount())) {
        m_activeBlockIndex = static_cast<size_t> (newIndex);
        refreshUI();
    }
}

bool EsbozoWindow::canFormat() const {
    if (!m_activeTextEdit || navigationLocked())
        return false;
    const Block* b = m_document.getBlock (m_activeBlockIndex);
    return b && InlineMarkup::isRichType (b->getType());
}

void EsbozoWindow::toggleBold() {
    if (!canFormat())
        return;
    QTextCharFormat fmt;
    bool on = m_activeTextEdit->currentCharFormat().fontWeight() >= QFont::Bold;
    fmt.setFontWeight (on ? QFont::Normal : QFont::Bold);
    m_activeTextEdit->mergeCurrentCharFormat (fmt);
}

void EsbozoWindow::toggleItalic() {
    if (!canFormat())
        return;
    QTextCharFormat fmt;
    fmt.setFontItalic (!m_activeTextEdit->currentCharFormat().fontItalic());
    m_activeTextEdit->mergeCurrentCharFormat (fmt);
}

QString EsbozoWindow::inputForProcessor() {
    QString plain = m_activeTextEdit->toPlainText();
    const Block* b = m_document.getBlock (m_activeBlockIndex);
    if (!b || !InlineMarkup::isRichType (b->getType()))
        return plain;

    QString cmd;
    const QString trimmed = plain.trimmed();
    for (const QString& c : CommandProcessor::getSupportedCommands()) {
        if (trimmed.endsWith (c)) {
            cmd = c;
            break;
        }
    }

    if (!cmd.isEmpty()) {
        QTextCursor cur (m_activeTextEdit->document());
        cur.setPosition (plain.lastIndexOf (cmd));
        cur.movePosition (QTextCursor::End, QTextCursor::KeepAnchor);
        cur.removeSelectedText();
    }
    return InlineMarkup::serialize (m_activeTextEdit->document()) + cmd;
}

void EsbozoWindow::updateStatus() {
    QString s = QString ("Bloque activo: %1 / %2").arg (m_activeBlockIndex + 1).arg (m_document.getBlockCount());
    if (canFormat()) {
        const QTextCharFormat f = m_activeTextEdit->currentCharFormat();
        if (f.fontWeight() >= QFont::Bold)
            s += "   [Negrita]";
        if (f.fontItalic())
            s += "   [Cursiva]";
    }
    m_statusLabel->setText (s);
}

void EsbozoWindow::updateImageWidths() {
    const int sideMargin = Theme::metrics().sideMargin;
    int available = m_scrollArea->width() - 2 * m_scrollArea->frameWidth() - 2 * sideMargin;
    int imageBoxWidth = qMax (100, available / 2);
    int wideBoxWidth = qMax (100, available * 3 / 4);

    for (int i = 0; i < m_documentLayout->count(); ++i) {
        QLayoutItem* item = m_documentLayout->itemAt (i);
        if (auto* img = qobject_cast<ImageLabel*> (item->widget()))
            img->setBoxWidth (imageBoxWidth);
        else if (auto* gr = qobject_cast<GraphLabel*> (item->widget()))
            gr->setBoxWidth (available * 3 / 4);
        else if (auto* eq = qobject_cast<EquationLabel*> (item->widget()))
            eq->setBoxWidth (available * 3 / 4);
        else if (auto* tb = qobject_cast<TableLabel*> (item->widget()))
            tb->setBoxWidth (wideBoxWidth);
    }
}

void EsbozoWindow::centerActiveBlock() {
    if (!m_containerWidget || !m_scrollArea)
        return;

    int viewportHeight = m_scrollArea->viewport()->height();
    int count = m_documentLayout->count();
    if (viewportHeight <= 0 || count == 0 || m_activeBlockIndex >= static_cast<size_t> (count))
        return;

    const int sideMargin = Theme::metrics().sideMargin;
    int contentWidth = m_scrollArea->viewport()->width() - 2 * sideMargin;
    int spacing = m_documentLayout->spacing();

    int hAbove = 0;
    int hActive = 50;
    for (int i = 0; i < count; ++i) {
        QLayoutItem* item = m_documentLayout->itemAt (i);
        int h = item->hasHeightForWidth() ? item->heightForWidth (contentWidth) : item->sizeHint().height();
        if (static_cast<size_t> (i) < m_activeBlockIndex) {
            hAbove += h + spacing;
        } else {
            hActive = h;
            break;
        }
    }

    int idealY = (viewportHeight / 2) - (hActive / 2);
    int topMargin = qMax (0, idealY - hAbove);
    int bottomMargin = qMax (0, (viewportHeight / 2) - (hActive / 2));

    m_documentLayout->setContentsMargins (sideMargin, topMargin, sideMargin, bottomMargin);

    int scroll = (topMargin == 0) ? (hAbove - idealY) : 0;
    m_scrollArea->verticalScrollBar()->setValue (scroll);
}

void EsbozoWindow::refreshUI() {
    QLayoutItem* child;
    while ((child = m_documentLayout->takeAt (0)) != nullptr) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    m_activeTextEdit = nullptr;
    const size_t count = m_document.getBlockCount();

    for (size_t i = 0; i < count; ++i) {
        Block* block = m_document.getBlock (i);
        if (!block)
            continue;

        if (i == m_activeBlockIndex)
            addEditor (block);
        else
            addInactiveWidget (i, block);
    }

    updateImageWidths();
    updateStatus();

    QTimer::singleShot (0, this, [this]() {
        centerActiveBlock();
    });
}

void EsbozoWindow::addEditor (Block* block) {
    const BlockType type = block->getType();

    QTextEdit* editBox = new QTextEdit (m_containerWidget);
    editBox->setAcceptRichText (false);

    if (InlineMarkup::isRichType (type))
        InlineMarkup::load (editBox->document(), block->getContent());
    else
        editBox->setPlainText (block->getContent());
    editBox->moveCursor (QTextCursor::End);

    editBox->setFont (Theme::editorFont (type));
    if (type == BlockType::Image && block->getContent().isEmpty())
        editBox->setPlaceholderText ("Ruta de la imagen");
    else if (type == BlockType::Equation && block->getContent().isEmpty())
        editBox->setPlaceholderText ("ecuación en LaTeX");

    editBox->setStyleSheet (Theme::editorStyle());
    editBox->setLineWrapMode (QTextEdit::WidgetWidth);
    editBox->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    editBox->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);

    auto updateHeight = [this, editBox]() {
        int docHeight = static_cast<int> (editBox->document()->size().height());
        int newHeight = qMax (50, docHeight + 16);
        editBox->setMinimumHeight (newHeight);
        editBox->setMaximumHeight (newHeight);
        centerActiveBlock();
    };

    connect (editBox->document()->documentLayout(),
             &QAbstractTextDocumentLayout::documentSizeChanged,
             this, [updateHeight] (const QSizeF&) {
                 updateHeight();
             });

    connect (editBox, &QTextEdit::textChanged, this, [this]() {
        saveCurrentContent();
    });

    connect (editBox, &QTextEdit::currentCharFormatChanged, this, [this] (const QTextCharFormat&) {
        updateStatus();
    });

    editBox->installEventFilter (this);
    m_documentLayout->addWidget (editBox);
    m_activeTextEdit = editBox;

    updateHeight();

    QTimer::singleShot (0, this, [this]() {
        if (m_activeTextEdit) {
            m_activeTextEdit->setFocus();
            m_activeTextEdit->moveCursor (QTextCursor::End);
        }
    });
}

void EsbozoWindow::addInactiveWidget (size_t index, Block* block) {
    const BlockType type = block->getType();
    ClickableLabel* clickable = nullptr;
    bool centered = true;

    if (type == BlockType::Image) {
        clickable = new ImageLabel (block->getContent(), m_containerWidget);
    } else if (type == BlockType::Graph) {
        const GraphBlock* graph = dynamic_cast<const GraphBlock*> (block);
        if (!graph)
            return;
        clickable = new GraphLabel (std::make_shared<GraphBlock> (*graph), m_containerWidget);
    } else if (type == BlockType::Equation) {
        int equationNumber = 0;
        for (size_t k = 0; k <= index; ++k) {
            if (m_document.getBlock (k)->getType() == BlockType::Equation)
                equationNumber++;
        }
        clickable = new EquationLabel (block->getContent(), equationNumber, m_containerWidget);
    } else if (type == BlockType::Table) {
        const TableBlock* table = dynamic_cast<const TableBlock*> (block);
        if (!table)
            return;
        clickable = new TableLabel (table->getData(), table->getTitle(), table->hasHeaderRow(),
                                    table->hasRowNames(), m_containerWidget);
    } else {
        const QString content = block->getContent();
        const QString html = content.isEmpty() ? QString ("(vacío)") : InlineMarkup::toHtml (content);

        clickable = new ClickableLabel (html, m_containerWidget);
        clickable->setTextFormat (Qt::RichText);
        clickable->setFont (Theme::labelFont (type));
        clickable->setStyleSheet (Theme::labelStyle (type));
        clickable->setWordWrap (true);
        centered = false;
    }

    connect (clickable, &ClickableLabel::doubleClicked, this, [this, index]() { activateBlock (index); });

    if (centered)
        m_documentLayout->addWidget (clickable, 0, Qt::AlignCenter);
    else
        m_documentLayout->addWidget (clickable);
}
