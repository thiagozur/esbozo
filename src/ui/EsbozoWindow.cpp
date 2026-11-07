#include "ui/EsbozoWindow.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCoreApplication>
#include <QEasingCurve>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <algorithm>
#include <memory>

#include "core/EquationBlock.hpp"
#include "core/GraphBlock.hpp"
#include "core/InlineMarkup.hpp"
#include "core/PdfExporter.hpp"
#include "core/TableBlock.hpp"
#include "core/TitleBlock.hpp"
#include "io/MarkdownExporter.hpp"
#include "io/ProjectFile.hpp"
#include "ui/Backdrop.hpp"
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

    m_backdrop = new Backdrop::Widget (this);
    QWidget* centralWidget = m_backdrop;
    QVBoxLayout* mainLayout = new QVBoxLayout (centralWidget);

    m_scrollArea = new QScrollArea (this);
    m_scrollArea->setWidgetResizable (true);

    m_containerWidget = new QWidget();
    m_documentLayout = new QVBoxLayout (m_containerWidget);
    m_documentLayout->setAlignment (Qt::AlignTop);
    m_documentLayout->setSpacing (Theme::metrics().blockSpacing);
    m_scrollArea->setWidget (m_containerWidget);
    m_containerWidget->setAutoFillBackground (false);
    m_scrollArea->viewport()->setAutoFillBackground (false);

    mainLayout->addWidget (m_scrollArea);

    m_statusLabel = new QLabel ("[Ctrl + Up / ,] subir, [Ctrl + Down / .] bajar. Enter procesa comandos.", this);
    m_statusLabel->setStyleSheet (Theme::statusStyle());
    m_statusLabel->setFont (Theme::labelSmallFont());
    mainLayout->addWidget (m_statusLabel);

    setCentralWidget (centralWidget);

    m_fade = new EdgeFade (m_scrollArea->viewport(), m_backdrop);
    m_fade->raise();
    m_scrollArea->viewport()->installEventFilter (this);

    m_transition = new QVariantAnimation (this);
    m_transition->setStartValue (0.0);
    m_transition->setEndValue (1.0);
    m_transition->setEasingCurve (QEasingCurve::OutCubic);
    connect (m_transition, &QVariantAnimation::valueChanged, this, [this] (const QVariant& value) {
        applyTransition (value.toReal());
    });
    connect (m_transition, &QVariantAnimation::finished, this, [this]() {
        finishTransition (true);
    });

    refreshUI (false);
}

int EsbozoWindow::verticalMargin() const {
    const Theme::Metrics& m = Theme::metrics();
    const int height = m_scrollArea->viewport()->height();
    const int wanted = qRound (m.pageMarginVerticalInches * logicalDpiY());
    int margin = qBound (0, (height - m.verticalMarginStartHeight) / 2, wanted);
    if (m.maxContentHeight > 0)
        margin = qMax (margin, (height - m.maxContentHeight) / 2);
    return margin;
}

void EsbozoWindow::setAnimationsEnabled (bool enabled) {
    m_animationsEnabled = enabled;
    if (!enabled)
        finishTransition (true);
}

bool EsbozoWindow::eventFilter (QObject* obj, QEvent* event) {
    if (obj == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        m_fade->setGeometry (m_scrollArea->viewport()->rect());
        m_fade->setMargin (verticalMargin());
        m_fade->raise();
    }

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
    m_activeTextEdit->setWizardMode (true);

    QString label;
    if (m_wizard.state() == Wizard::State::ExportFormat) {
        label = "exportar";
    } else if (const Block* block = m_document.getBlock (m_activeBlockIndex)) {
        label = commandLabelFor (block->getType());
        const Wizard::Progress p = m_wizard.progress();
        if (p.total > 1)
            label += QString ("  ·  %1/%2").arg (p.step).arg (p.total);
    }
    m_activeTextEdit->setLabelText (label);
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
    refreshUI (false);
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
    refreshUI (false);
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
    if (navigationLocked() || m_syncing)
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

int EsbozoWindow::sideMargin() const {
    const Theme::Metrics& m = Theme::metrics();
    const int wanted = qRound (m.pageMarginInches * logicalDpiX());
    int margin = qMax (m.sideMargin, qMin (wanted, width() / 8));
    if (m.maxContentWidth > 0)
        margin = qMax (margin, (width() - m.maxContentWidth) / 2);
    return margin;
}

void EsbozoWindow::updateImageWidths() {
    int available = m_scrollArea->width() - 2 * m_scrollArea->frameWidth() - 2 * sideMargin();
    const int imageBox = qMax (100, available / 2);
    const int wideBox = qMax (100, available * 3 / 4);

    for (BlockView* view : m_views)
        view->setCardWidths (imageBox, wideBox);
}

void EsbozoWindow::centerActiveBlock() {
    if (m_centering || !m_containerWidget || !m_scrollArea)
        return;

    const int viewportHeight = m_scrollArea->viewport()->height();
    const int count = m_documentLayout->count();
    if (viewportHeight <= 0 || count == 0 || m_activeBlockIndex >= static_cast<size_t> (count))
        return;

    m_centering = true;

    const int margin = sideMargin();
    const int contentWidth = m_scrollArea->viewport()->width() - 2 * margin;
    const int spacing = m_documentLayout->spacing();

    int hAbove = 0;
    int hActive = 50;
    for (int i = 0; i < count; ++i) {
        QLayoutItem* item = m_documentLayout->itemAt (i);
        const int h = item->hasHeightForWidth() ? item->heightForWidth (contentWidth) : item->sizeHint().height();
        if (static_cast<size_t> (i) < m_activeBlockIndex) {
            hAbove += h + spacing;
        } else {
            hActive = h;
            break;
        }
    }

    m_documentLayout->setContentsMargins (margin, viewportHeight / 2, margin, viewportHeight / 2);
    QCoreApplication::sendPostedEvents (m_containerWidget, QEvent::LayoutRequest);

    const int ideal = hAbove + hActive / 2;
    int value = ideal;
    if (m_transitioning)
        value = qRound (m_scrollStart + (ideal - m_scrollStart) * m_progress);
    m_scrollArea->verticalScrollBar()->setValue (value);

    m_centering = false;
}

void EsbozoWindow::startTransition (BlockView* from, BlockView* to) {
    finishTransition (false);

    m_fadeOut = from;
    m_fadeIn = to;
    m_scrollStart = m_scrollArea->verticalScrollBar()->value();
    m_transitioning = true;
    m_progress = 0.0;

    if (m_fadeOut)
        m_fadeOut->setFocusProgress (1.0);
    if (m_fadeIn)
        m_fadeIn->setFocusProgress (0.0);

    m_transition->setDuration (Theme::metrics().transitionMs);
    m_transition->start();
}

void EsbozoWindow::applyTransition (qreal progress) {
    m_progress = progress;
    if (m_fadeOut)
        m_fadeOut->setFocusProgress (1.0 - progress);
    if (m_fadeIn)
        m_fadeIn->setFocusProgress (progress);
    centerActiveBlock();
}

void EsbozoWindow::finishTransition (bool recenter) {
    if (m_transition->state() == QAbstractAnimation::Running)
        m_transition->stop();

    if (m_fadeOut)
        m_fadeOut->setFocusProgress (0.0);
    if (m_fadeIn)
        m_fadeIn->setFocusProgress (1.0);

    m_fadeOut = nullptr;
    m_fadeIn = nullptr;
    m_transitioning = false;
    m_progress = 1.0;

    if (recenter)
        centerActiveBlock();
}

int EsbozoWindow::indexOfView (const BlockView* view) const {
    for (size_t i = 0; i < m_views.size(); ++i) {
        if (m_views[i] == view)
            return static_cast<int> (i);
    }
    return -1;
}

BlockView* EsbozoWindow::createView (Block* block) {
    BlockView* view = new BlockView (block, m_containerWidget);
    view->editor()->installEventFilter (this);

    connect (view->editor(), &QTextEdit::textChanged, this, [this, view]() {
        if (view->editor() == m_activeTextEdit)
            saveCurrentContent();
    });

    connect (view->editor(), &QTextEdit::currentCharFormatChanged, this, [this, view] (const QTextCharFormat&) {
        if (view->editor() == m_activeTextEdit)
            updateStatus();
    });

    connect (view, &BlockView::activated, this, [this, view]() {
        const int index = indexOfView (view);
        if (index >= 0)
            activateBlock (static_cast<size_t> (index));
    });

    connect (view, &BlockView::contentHeightChanged, this, [this]() {
        if (!m_syncing)
            centerActiveBlock();
    });

    return view;
}

void EsbozoWindow::refreshUI (bool animate) {
    finishTransition (false);
    m_syncing = true;

    const size_t count = m_document.getBlockCount();
    QHash<const Block*, BlockView*> reusable;
    QSet<const Block*> alive;
    for (size_t i = 0; i < count; ++i)
        alive.insert (m_document.getBlock (i));

    std::vector<BlockView*> stale;
    for (BlockView* view : m_views) {
        if (alive.contains (view->block()) && view->block()->getType() == view->type())
            reusable.insert (view->block(), view);
        else
            stale.push_back (view);
    }

    std::vector<BlockView*> views;
    for (size_t i = 0; i < count; ++i) {
        Block* block = m_document.getBlock (i);
        BlockView* view = reusable.value (block, nullptr);
        views.push_back (view ? view : createView (block));
    }

    bool sameOrder = m_documentLayout->count() == static_cast<int> (views.size());
    for (size_t i = 0; sameOrder && i < views.size(); ++i)
        sameOrder = m_documentLayout->itemAt (static_cast<int> (i))->widget() == views[i];
    if (!sameOrder) {
        while (QLayoutItem* item = m_documentLayout->takeAt (0))
            delete item;
        for (BlockView* view : views)
            m_documentLayout->addWidget (view);
    }

    for (BlockView* view : stale) {
        view->hide();
        view->deleteLater();
    }
    m_views = views;

    int equationNumber = 0;
    for (size_t i = 0; i < m_views.size(); ++i) {
        BlockView* view = m_views[i];
        if (view->type() == BlockType::Equation)
            view->setEquationNumber (++equationNumber);
        view->syncFromBlock();
        const bool active = (i == m_activeBlockIndex);
        view->setFocusedLook (active, active);
    }

    BlockView* next = m_activeBlockIndex < m_views.size() ? m_views[m_activeBlockIndex] : nullptr;
    BlockView* previous = m_activeView;
    if (previous && std::find (m_views.begin(), m_views.end(), previous) == m_views.end())
        previous = nullptr;
    m_activeView = next;

    m_activeTextEdit = next ? next->editor() : nullptr;
    m_syncing = false;

    updateImageWidths();
    updateStatus();

    if (animate && m_animationsEnabled && isVisible() && next && next != previous)
        startTransition (previous, next);

    QTimer::singleShot (0, this, [this]() {
        centerActiveBlock();
        if (m_activeTextEdit) {
            m_activeTextEdit->setFocus();
            m_activeTextEdit->moveCursor (QTextCursor::End);
        }
    });
}
