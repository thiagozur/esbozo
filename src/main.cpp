#include <QApplication>
#include <QMainWindow>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QWidget>
#include <QKeyEvent>
#include <QEvent>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>

#include "core/Document.hpp"
#include "core/CommandProcessor.hpp"

class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableLabel (const QString &text, QWidget *parent = nullptr) : QLabel (text, parent) {}

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent (QMouseEvent *event) override {
        QLabel::mouseDoubleClickEvent (event);
        emit doubleClicked();
    }
};

class EsbozoWindow : public QMainWindow {
    Q_OBJECT

private:
    Document m_document;
    CommandProcessor m_processor;
    size_t m_activeBlockIndex;

    QWidget *m_containerWidget;
    QVBoxLayout *m_documentLayout;
    QScrollArea *m_scrollArea;
    QLabel *m_statusLabel;
    QTextEdit *m_activeTextEdit = nullptr;

public:
    EsbozoWindow() : m_processor (m_document), m_activeBlockIndex (0) {
        setWindowTitle ("Esbozo");
        resize (800, 600);

        if (m_document.getBlockCount() == 0) {
            m_document.addBlock(std::make_unique<ParagraphBlock> (false));
        }

        QWidget *centralWidget = new QWidget (this);
        QVBoxLayout *mainLayout = new QVBoxLayout (centralWidget);

        m_scrollArea = new QScrollArea (this);
        m_scrollArea->setWidgetResizable (true);
        
        m_containerWidget = new QWidget();
        m_documentLayout = new QVBoxLayout (m_containerWidget);
        m_documentLayout->setAlignment (Qt::AlignTop);
        m_documentLayout->setSpacing (16);
        m_scrollArea->setWidget (m_containerWidget);

        mainLayout->addWidget (m_scrollArea);

        m_statusLabel = new QLabel ("[Ctrl + Up / ,] subir, [Ctrl + Down / .] bajar. Enter procesa comandos.", this);
        m_statusLabel->setStyleSheet ("color: gray; font-style: italic;");
        mainLayout->addWidget (m_statusLabel);

        setCentralWidget (centralWidget);

        refreshUI();
    }

protected:
    bool eventFilter (QObject *obj, QEvent *event) override {
        if (obj == m_activeTextEdit && event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*> (event);

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
            }
            
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                QString text = m_activeTextEdit->toPlainText();
                QString trimmedText = text.trimmed();
                
                QStringList possibleCmds = CommandProcessor::getSupportedCommands();
                bool hasCommand = false;
                for (const QString& cmd : possibleCmds) {
                    if (trimmedText.endsWith (cmd)) {
                        hasCommand = true;
                        break;
                    }
                }

                if (hasCommand) {
                    handleInputSubmitted();
                    return true;
                }
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

    void keyPressEvent (QKeyEvent *event) override {
        if (event->modifiers() == Qt::ControlModifier) {
            if (event->key() == Qt::Key_Comma || event->key() == Qt::Key_Up) {
                saveCurrentContent();
                saveAndNavigate(-1);
                event->accept();
                return;
            } 
            else if (event->key() == Qt::Key_Period || event->key() == Qt::Key_Down) {
                saveCurrentContent();
                saveAndNavigate(1);
                event->accept();
                return;
            }
        }
        QMainWindow::keyPressEvent (event);
    }

    void resizeEvent (QResizeEvent *event) override {
        QMainWindow::resizeEvent (event);
        centerActiveBlock();
    }

private slots:
    void handleInputSubmitted() {
        if (!m_activeTextEdit) return;

        QString text = m_activeTextEdit->toPlainText();
        
        m_processor.setActiveBlockIndex (m_activeBlockIndex);
        CommandResult result = m_processor.processInput (text);

        if (result == CommandResult::BlockCreated || result == CommandResult::SectionFinished || result == CommandResult::BlockDeleted) {
            m_activeBlockIndex = m_processor.getActiveBlockIndex();
        }

        refreshUI();
    }

private:
    void saveCurrentContent() {
        if (m_activeTextEdit && m_activeBlockIndex < m_document.getBlockCount()) {
            if (Block* current = m_document.getBlock (m_activeBlockIndex)) {
                current->setContent (m_activeTextEdit->toPlainText());
            }
        }
    }

    void saveAndNavigate(int delta) {
        saveCurrentContent();

        long long newIndex = static_cast<long long>(m_activeBlockIndex) + delta;
        if (newIndex >= 0 && newIndex < static_cast<long long> (m_document.getBlockCount())) {
            m_activeBlockIndex = static_cast<size_t> (newIndex);
            refreshUI();
        }
    }

    void centerActiveBlock() {
        if (!m_containerWidget || !m_scrollArea) return;
        
        int viewportHeight = m_scrollArea->viewport()->height();
        if (viewportHeight <= 0) return;

        int count = static_cast<int> (m_document.getBlockCount());
        if (count == 0) return;

        m_containerWidget->adjustSize();

        QWidget *activeWidget = nullptr;
        int hAbove = 0;
        int spacing = m_documentLayout->spacing();

        for (int i = 0; i < count; ++i) {
            QLayoutItem *item = m_documentLayout->itemAt (i);
            if (!item || !item->widget()) continue;
            QWidget *w = item->widget();
            if (static_cast<size_t> (i) < m_activeBlockIndex) {
                hAbove += w->height() + spacing;
            } else if (static_cast<size_t> (i) == m_activeBlockIndex) {
                activeWidget = w;
                break;
            }
        }

        int hActive = activeWidget ? activeWidget->height() : 50;
        int idealY = (viewportHeight / 2) - (hActive / 2);
        int topMargin = qMax (0, idealY - hAbove);
        int bottomMargin = qMax (0, (viewportHeight / 2) - (hActive / 2));

        m_documentLayout->setContentsMargins (20, topMargin, 20, bottomMargin);
        m_containerWidget->adjustSize();

        if (topMargin == 0 && activeWidget) {
            int targetScroll = activeWidget->y() + (activeWidget->height() / 2) - (viewportHeight / 2);
            m_scrollArea->verticalScrollBar()->setValue (targetScroll);
        } else {
            m_scrollArea->verticalScrollBar()->setValue (0);
        }
    }

    void refreshUI() {
        QLayoutItem *child;
        while ((child = m_documentLayout->takeAt (0)) != nullptr) {
            if (child->widget()) {
                child->widget()->deleteLater();
            }
            delete child;
        }

        m_activeTextEdit = nullptr;
        size_t count = m_document.getBlockCount();

        for (size_t i = 0; i < count; ++i) {
            Block* block = m_document.getBlock (i);
            if (!block) continue;

            if (i == m_activeBlockIndex) {
                QTextEdit *editBox = new QTextEdit (m_containerWidget);
                editBox->setPlainText (block->getContent());
                editBox->moveCursor (QTextCursor::End);
                editBox->setAcceptRichText (false);
                editBox->setFont (QFont ("Segoe UI", 13, QFont::Bold));
                editBox->setStyleSheet ("QTextEdit { background-color: #F0F8FF; border: 2px solid #4682B4; padding: 6px; border-radius: 4px; }");
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
                        this, [updateHeight] (const QSizeF &) { updateHeight(); });

                connect (editBox, &QTextEdit::textChanged, this, [this]() {
                    saveCurrentContent();
                });

                editBox->installEventFilter (this);
                m_documentLayout->addWidget (editBox);
                m_activeTextEdit = editBox;

                updateHeight();
                
                QTimer::singleShot(0, this, [this]() {
                    if (m_activeTextEdit) {
                        m_activeTextEdit->setFocus();
                        m_activeTextEdit->moveCursor(QTextCursor::End);
                    }
                });
            } else {
                QString content = block->getContent();
                if (content.isEmpty()) {
                    content = "(vacío)";
                } else {
                    content = content.toHtmlEscaped();
                    content.replace ("\n\n", "<br>&nbsp;<br>");
                    content.replace ("\n", "<br>");
                }

                ClickableLabel *label = new ClickableLabel (content, m_containerWidget);
                label->setFont (QFont ("Segoe UI", 11));
                label->setStyleSheet ("QLabel { color: #555555; padding: 6px; }");
                label->setWordWrap (true);
                
                connect (label, &ClickableLabel::doubleClicked, this, [this, i]() {
                    saveCurrentContent();
                    m_activeBlockIndex = i;
                    refreshUI();
                });

                m_documentLayout->addWidget (label);
            }
        }

        m_statusLabel->setText (QString ("Bloque activo: %1 / %2").arg (m_activeBlockIndex + 1).arg (count));
        
        QTimer::singleShot (0, this, [this]() {
            centerActiveBlock();
        });
    }
};

int main (int argc, char *argv[]) {
    QApplication app (argc, argv);

    EsbozoWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"