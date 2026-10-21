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
#include <QFileDialog>

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

class ImageLabel : public ClickableLabel {
    Q_OBJECT
private:
    QPixmap m_original;
    int m_lastWidth = -1;
    static constexpr int kChrome = 2 * (8 + 1);

public:
    explicit ImageLabel (const QString &path, QWidget *parent = nullptr)
        : ClickableLabel ("", parent), m_original (path)
    {
        setAlignment (Qt::AlignCenter);

        if (path.isEmpty()) {
            setText ("(imagen vacía)");
            setStyleSheet ("QLabel { color: #888888; font-style: italic; padding: 6px; }");
        } else if (m_original.isNull()) {
            setText ("[Error cargando imagen: " + path + "]");
            setStyleSheet ("QLabel { color: #CC0000; font-weight: bold; padding: 6px; }");
        } else {
            setStyleSheet ("QLabel { padding: 8px; background-color: #FAFAFA; border: 1px solid #E0E0E0; border-radius: 4px; }");
        }
    }

    void setBoxWidth (int boxWidth) {
        if (m_original.isNull()) return;

        int pixWidth = qMax (50, boxWidth - kChrome);
        if (pixWidth == m_lastWidth) return;
        m_lastWidth = pixWidth;

        QPixmap scaled = m_original.scaledToWidth (pixWidth, Qt::SmoothTransformation);
        setPixmap (scaled);
        setFixedSize (scaled.width() + kChrome, scaled.height() + kChrome);
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
            m_document.addBlock(std::make_unique<TitleBlock>());
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
        }
        QMainWindow::keyPressEvent (event);
    }

    void resizeEvent (QResizeEvent *event) override {
        QMainWindow::resizeEvent (event);
        updateImageWidths();
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

        if (Block* currentBlock = m_document.getBlock(m_activeBlockIndex)) {
            if (currentBlock->getType() == BlockType::Image && currentBlock->getContent().isEmpty()) {
                QFileDialog dialog (this, "Seleccionar Imagen");
                dialog.setOption (QFileDialog::DontUseNativeDialog, true);
                dialog.setFileMode (QFileDialog::ExistingFile);
                dialog.setNameFilter ("Imágenes (*.png *.jpg *.jpeg)");
                dialog.setViewMode (QFileDialog::Detail);
                dialog.setStyleSheet(
                    "QDialog { background-color: #Fdfdfd; }"
                    "QLabel { color: #333333; font-family: 'Segoe UI'; }"
                    "QPushButton { background-color: #4682B4; color: white; border-radius: 4px; padding: 6px 12px; font-family: 'Segoe UI'; }"
                    "QPushButton:hover { background-color: #3b6e9b; }"
                );
                
                if (dialog.exec() == QDialog::Accepted) {
                    QStringList files = dialog.selectedFiles();
                    if (!files.isEmpty()) {

                        currentBlock->setContent (files.first());
                        refreshUI(); 
                    }
                } else {
                    if (m_document.getBlockCount() > 1) {
                        size_t indexToDelete = m_activeBlockIndex;
                        if (m_activeBlockIndex > 0) {
                            m_activeBlockIndex--;
                        }
                        m_document.removeBlock (indexToDelete);
                    } else {
                        currentBlock->setContent ("");
                    }
                    refreshUI();
                }
            }
        }
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

    void updateImageWidths() {
        int available = m_scrollArea->width() - 2 * m_scrollArea->frameWidth() - 40;
        int boxWidth = qMax (100, available / 2);

        for (int i = 0; i < m_documentLayout->count(); ++i) {
            QLayoutItem *item = m_documentLayout->itemAt (i);
            if (auto *img = qobject_cast<ImageLabel*> (item->widget())) {
                img->setBoxWidth (boxWidth);
            }
        }
    }

    void centerActiveBlock() {
        if (!m_containerWidget || !m_scrollArea) return;

        int viewportHeight = m_scrollArea->viewport()->height();
        int count = m_documentLayout->count();
        if (viewportHeight <= 0 || count == 0 || m_activeBlockIndex >= static_cast<size_t> (count)) return;

        const int sideMargin = 20;
        int contentWidth = m_scrollArea->viewport()->width() - 2 * sideMargin;
        int spacing = m_documentLayout->spacing();

        int hAbove = 0;
        int hActive = 50;
        for (int i = 0; i < count; ++i) {
            QLayoutItem *item = m_documentLayout->itemAt (i);
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

            BlockType type = block->getType();

            if (i == m_activeBlockIndex) {
                QTextEdit *editBox = new QTextEdit (m_containerWidget);
                editBox->setPlainText (block->getContent());
                editBox->moveCursor (QTextCursor::End);
                editBox->setAcceptRichText (false);
                
                if (type == BlockType::Title) {
                    editBox->setFont (QFont ("Segoe UI", 16, QFont::Bold));
                } else if (type == BlockType::Subtitle) {
                    editBox->setFont (QFont ("Segoe UI", 14, QFont::DemiBold));
                } else if (type == BlockType::Image) {
                    editBox->setFont (QFont ("Segoe UI", 11, QFont::StyleItalic));
                    if (block->getContent().isEmpty()) {
                        editBox->setPlaceholderText ("Ruta de la imagen...");
                    }
                } else {
                    editBox->setFont (QFont ("Segoe UI", 13, QFont::Normal));
                }

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
                if (type == BlockType::Image) {
                    QString path = block->getContent();
                    
                    ImageLabel *label = new ImageLabel (path, m_containerWidget);

                    connect (label, &ImageLabel::doubleClicked, this, [this, i]() {
                        saveCurrentContent();
                        m_activeBlockIndex = i;
                        refreshUI();
                    });

                    m_documentLayout->addWidget (label, 0, Qt::AlignCenter);
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
                    
                    if (type == BlockType::Title) {
                        label->setFont (QFont ("Segoe UI", 15, QFont::Bold));
                        label->setStyleSheet ("QLabel { color: #222222; padding: 6px; }");
                    } else if (type == BlockType::Subtitle) {
                        label->setFont (QFont ("Segoe UI", 13, QFont::DemiBold));
                        label->setStyleSheet ("QLabel { color: #444444; padding: 6px; }");
                    } else {
                        label->setFont (QFont ("Segoe UI", 11, QFont::Normal));
                        label->setStyleSheet ("QLabel { color: #555555; padding: 6px; }");
                    }

                    label->setWordWrap (true);
                    
                    connect (label, &ClickableLabel::doubleClicked, this, [this, i]() {
                        saveCurrentContent();
                        m_activeBlockIndex = i;
                        refreshUI();
                    });

                    m_documentLayout->addWidget (label);
                }
            }
        }

        updateImageWidths();

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