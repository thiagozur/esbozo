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
#include <QRegularExpression>
#include <QFile>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QFont>

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

enum class GraphWizardState {
    Idle,
    WaitingForType,
    WaitingForSource,
    WaitingForXName,
    WaitingForXValues,
    WaitingForYName,
    WaitingForYValues,
    WaitingForXScale,
    WaitingForYScale,
    WaitingForDescription
};

class EsbozoWindow : public QMainWindow {
    Q_OBJECT

private:
    Document m_document;
    CommandProcessor m_processor;
    size_t m_activeBlockIndex;

    GraphWizardState m_graphState = GraphWizardState::Idle;
    GraphType m_tempGraphType = GraphType::Line;
    GraphSource m_tempGraphSource = GraphSource::Manual;
    QString m_tempXName, m_tempYName;
    QVector<double> m_tempXVals, m_tempYVals;
    AxisScale m_tempXScale = AxisScale::Linear;
    AxisScale m_tempYScale = AxisScale::Linear;

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
                if (m_graphState != GraphWizardState::Idle) {
                    handleInputSubmitted();
                    return true;
                }
                
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

        if (m_graphState != GraphWizardState::Idle) {
            QString trimmedText = text.trimmed();

            if (trimmedText == "can;;") {
                m_graphState = GraphWizardState::Idle;
                if (m_document.getBlockCount() > 1) {
                    size_t indexToDelete = m_activeBlockIndex;
                    if (m_activeBlockIndex > 0) {
                        m_activeBlockIndex--;
                    }
                    m_document.removeBlock (indexToDelete);
                } else {
                    if (auto* currentBlock = m_document.getBlock (m_activeBlockIndex)) {
                        currentBlock->setContent("");
                    }
                }
                refreshUI();
                return;
            }

            switch (m_graphState) {
                case GraphWizardState::WaitingForType:
                    if (trimmedText == "lin")
                        m_tempGraphType = GraphType::Line;
                    else if (trimmedText == "sct") 
                        m_tempGraphType = GraphType::Scatter;
                    else {
                        m_activeTextEdit->clear();
                        return;
                    }
                    
                    m_graphState = GraphWizardState::WaitingForSource;
                    updateGraphPrompt("csv (archivo) o man (entrada manual)");
                    break;
                
                case GraphWizardState::WaitingForSource:
                    if (trimmedText == "csv") {
                        m_tempGraphSource = GraphSource::Csv;
                        openCsvFileDialog();
                    } else if (trimmedText == "man") {
                        m_tempGraphSource = GraphSource::Manual;
                        m_graphState = GraphWizardState::WaitingForXName;
                        updateGraphPrompt ("Nombre de la serie X");
                    } else {
                        m_activeTextEdit->clear();
                        return;
                    }
                    break;

                case GraphWizardState::WaitingForXName:
                    if (trimmedText.isEmpty()) {
                        m_activeTextEdit->clear();
                        return;
                    }
                    m_tempXName = trimmedText;
                    m_graphState = GraphWizardState::WaitingForXValues;
                    updateGraphPrompt ("Valores de X separados por comas");
                    break;

                case GraphWizardState::WaitingForXValues:
                    m_tempXVals = parseCommaValues (trimmedText);
                    if (m_tempXVals.isEmpty()) {
                        m_activeTextEdit->clear();
                        return;
                    }
                    m_graphState = GraphWizardState::WaitingForYName;
                    updateGraphPrompt ("Nombre de la serie Y");
                    break;

                case GraphWizardState::WaitingForYName:
                    if (trimmedText.isEmpty()) {
                        m_activeTextEdit->clear();
                        return;
                    }
                    m_tempYName = trimmedText;
                    m_graphState = GraphWizardState::WaitingForYValues;
                    updateGraphPrompt ("Valores de Y separados por comas");
                    break;

                case GraphWizardState::WaitingForYValues:
                    m_tempYVals = parseCommaValues (trimmedText);
                    if (m_tempYVals.isEmpty() || m_tempYVals.size() != m_tempXVals.size()) {
                        m_activeTextEdit->clear();
                        return;
                    }
                    m_graphState = GraphWizardState::WaitingForXScale;
                    updateGraphPrompt ("lin (eje x lineal) o log (eje x logarítmico)");
                    break;

                case GraphWizardState::WaitingForXScale:
                    if (trimmedText == "lin")
                        m_tempXScale = AxisScale::Linear;
                    else if (trimmedText == "log")
                        m_tempXScale = AxisScale::Log;
                    else {
                        m_activeTextEdit->clear();
                        return;
                    }

                    m_graphState = GraphWizardState::WaitingForYScale;
                    updateGraphPrompt ("lin (eje y lineal) o log (eje y logarítmico)");
                    break;
                
                case GraphWizardState::WaitingForYScale:
                    if (trimmedText == "lin")
                        m_tempYScale = AxisScale::Linear;
                    else if (trimmedText == "log")
                        m_tempYScale = AxisScale::Log;
                    else {
                        m_activeTextEdit->clear();
                        return;
                    }

                    m_graphState = GraphWizardState::WaitingForDescription;
                    updateGraphPrompt ("descripción de la figura");
                    break;

                case GraphWizardState::WaitingForDescription:
                    {
                        if (auto* gBlock = dynamic_cast<GraphBlock*> (m_document.getBlock (m_activeBlockIndex))) {
                            gBlock->setGraphType (m_tempGraphType);
                            gBlock->setGraphSource (m_tempGraphSource);
                            gBlock->setSeriesX (m_tempXName, m_tempXVals);
                            gBlock->setSeriesY (m_tempYName, m_tempYVals);
                            gBlock->setScales (m_tempXScale, m_tempYScale);
                            gBlock->setDescription (trimmedText);
                        }
                        m_graphState = GraphWizardState::Idle;
                        refreshUI();
                    }
                    break;
                
                default:
                    break;
            }
            return;
        }
        
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
            else if (currentBlock->getType() == BlockType::Graph && m_graphState == GraphWizardState::Idle) {
                m_graphState = GraphWizardState::WaitingForType;
                updateGraphPrompt ("lin (línea) o sct (dispersión)");
            }
        }
    }

private:
    QPixmap renderGraphPixmap (const GraphBlock* gBlock, int targetWidth, int targetHeight) {
        QPixmap pixmap (targetWidth, targetHeight);
        pixmap.fill (Qt::white);

        QPainter painter (&pixmap);
        painter.setRenderHint (QPainter::Antialiasing, true);

        int plotX = 60;
        int plotY = 30;
        int plotW = targetWidth - 90;
        int plotH = targetHeight - 80;

        painter.setPen (QPen (QColor ("#E0E0E0"), 1));
        painter.drawRect (plotX, plotY, plotW, plotH);

        const auto& xVals = gBlock->getSeriesX().values;
        const auto& yVals = gBlock->getSeriesY().values;
        if (xVals.isEmpty() || yVals.isEmpty() || xVals.size() != yVals.size()) {
            painter.setPen (Qt::black);
            painter.drawText (pixmap.rect(), Qt::AlignCenter, "Sin datos para graficar");
            return pixmap;
        }

        double minX = xVals[0], maxX = xVals[0];
        double minY = yVals[0], maxY = yVals[0];
        for (int i = 0; i < xVals.size(); ++i) {
            minX = qMin (minX, xVals[i]); maxX = qMax (maxX, xVals[i]);
            minY = qMin (minY, yVals[i]); maxY = qMax (maxY, yVals[i]);
        }
        if (qFuzzyCompare (minX, maxX))
            maxX += 1.0;
        if (qFuzzyCompare (minY, maxY))
            maxY += 1.0;

        auto mapX = [&] (double val) {
            return plotX + static_cast<int> ((val - minX) / (maxX - minX) * plotW);
        };
        auto mapY = [&] (double val) {
            return plotY + plotH - static_cast<int> ((val - minY) / (maxY - minY) * plotH);
        };

        painter.setPen (QPen(Qt::black, 1));
        painter.setFont (QFont ("Segoe UI", 8));

        for (double xVal : xVals) {
            int px = mapX (xVal);
            painter.drawLine (px, plotY + plotH, px, plotY + plotH + 5);
            QString labelStr = QString::number (xVal, 'g', 3);
            painter.drawText (px - 15, plotY + plotH + 8, 30, 15, Qt::AlignCenter, labelStr);
        }

        for (double yVal : yVals) {
            int py = mapY (yVal);
            painter.drawLine (plotX - 5, py, plotX, py);
            QString labelStr = QString::number (yVal, 'g', 3);
            painter.drawText (plotX - 55, py - 8, 50, 15, Qt::AlignRight | Qt::AlignVCenter, labelStr);
        }

        if (gBlock->getGraphType() == GraphType::Line) {
            painter.setPen (QPen (QColor ("#4682B4"), 2));
            for (int i = 0; i < xVals.size() - 1; ++i) {
                painter.drawLine (mapX (xVals[i]), mapY (yVals[i]), mapX (xVals[i+1]), mapY (yVals[i+1]));
            }
        } else {
            painter.setBrush (QColor("#E67E22"));
            painter.setPen (Qt::NoPen);
            for (int i = 0; i < xVals.size(); ++i) {
                painter.drawEllipse (QPoint (mapX (xVals[i]), mapY (yVals[i])), 4, 4);
            }
        }

        painter.setPen (Qt::black);
        painter.setFont (QFont ("Segoe UI", 9));
        painter.drawText (plotX + plotW / 2 - 40, targetHeight - 20, gBlock->getSeriesX().name);
        
        painter.setFont (QFont ("Segoe UI", 10, QFont::Bold));
        painter.drawText (plotX, targetHeight - 8, gBlock->getDescription());

        return pixmap;
    }

    void updateGraphPrompt (const QString& promptText) {
        if (m_activeTextEdit) {
            m_activeTextEdit->clear();
            m_activeTextEdit->setPlaceholderText (promptText);
            m_activeTextEdit->setStyleSheet ("QTextEdit { background-color: #FEF9E7; border: 2px solid #E67E22; padding: 6px; border-radius: 4px; }");
        }
    }

    QVector<double> parseCommaValues (const QString& str) {
        QVector<double> result;
        QStringList parts = str.split (',');
        for (const QString& p : parts) {
            bool ok = false;
            double val = p.trimmed().toDouble (&ok);
            if (ok) result.append (val);
        }
        return result;
    }

    bool parseCsvFile (const QString& path) {
        QFile file (path);
        if (!file.open (QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }

        QTextStream in (&file);
        if (in.atEnd()) return false;

        QString headerLine = in.readLine();
        QStringList headers = headerLine.split (QRegularExpression ("[,;\\t]"), Qt::SkipEmptyParts);
        if (headers.size() < 2) return false;

        m_tempXName = headers[0].trimmed();
        m_tempYName = headers[1].trimmed();

        m_tempXVals.clear();
        m_tempYVals.clear();

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            QStringList parts = line.split (QRegularExpression ("[,;\\t]"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                bool okX = false, okY = false;
                double xVal = parts[0].trimmed().toDouble (&okX);
                double yVal = parts[1].trimmed().toDouble (&okY);
                if (okX && okY) {
                    m_tempXVals.append (xVal);
                    m_tempYVals.append (yVal);
                }
            }
        }

        file.close();
        return !m_tempXVals.isEmpty() && !m_tempYVals.isEmpty();
    }

    void openCsvFileDialog() {
        QFileDialog dialog (this, "Seleccionar Archivo CSV");
        dialog.setOption (QFileDialog::DontUseNativeDialog, true);
        dialog.setFileMode (QFileDialog::ExistingFile);
        dialog.setNameFilter  ("Archivos CSV (*.csv)");
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
                QString path = files.first();
                if (parseCsvFile (path)) {
                    m_graphState = GraphWizardState::WaitingForXScale;
                    updateGraphPrompt ("Escala eje X: escribe 'lin' o 'log'...");
                } else {
                    updateGraphPrompt ("Error al leer el CSV. Escribe 'can;;' para cancelar o reintenta.");
                }
            }
        } else {
            m_graphState = GraphWizardState::Idle;
            refreshUI();
        }
    }

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
                } else if (type == BlockType::Graph) {
                    if (const GraphBlock* gBlock = dynamic_cast<const GraphBlock*> (block)) {
                        QPixmap graphPix = renderGraphPixmap (gBlock, 600, 350);

                        ClickableLabel *graphLabel = new ClickableLabel("", m_containerWidget);
                        graphLabel->setPixmap (graphPix);
                        graphLabel->setAlignment (Qt::AlignCenter);
                        graphLabel->setStyleSheet ("QLabel { padding: 8px; background-color: #FAFAFA; border: 1px solid #E0E0E0; border-radius: 4px; }");

                        connect (graphLabel, &ClickableLabel::doubleClicked, this, [this, i]() {
                            saveCurrentContent();
                            m_activeBlockIndex = i;
                            refreshUI();
                        });

                        m_documentLayout->addWidget (graphLabel, 0, Qt::AlignCenter);
                    }
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