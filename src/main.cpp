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
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QFont>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextCharFormat>
#include <memory>
#include <limits>

#include "core/Document.hpp"
#include "core/CommandProcessor.hpp"
#include "core/GraphRenderer.hpp"
#include "core/EquationRenderer.hpp"
#include "core/InlineMarkup.hpp"

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

class GraphLabel : public ClickableLabel {
    Q_OBJECT
private:
    std::shared_ptr<const GraphBlock> m_graph;
    int m_lastWidth = -1;
    qreal m_lastDpr = 0.0;
    static constexpr int kChrome = 2 * (8 + 1);

public:
    explicit GraphLabel (std::shared_ptr<const GraphBlock> graph, QWidget *parent = nullptr)
        : ClickableLabel ("", parent), m_graph (std::move (graph))
    {
        setAlignment (Qt::AlignCenter);
        setStyleSheet ("QLabel { padding: 8px; background-color: #FAFAFA; border: 1px solid #E0E0E0; border-radius: 4px; }");
    }

    void setBoxWidth (int boxWidth) {
        int pixWidth = qMax (260, boxWidth - kChrome);
        qreal dpr = devicePixelRatioF();
        if (pixWidth == m_lastWidth && dpr == m_lastDpr)
            return;
        m_lastWidth = pixWidth;
        m_lastDpr = dpr;

        int pixHeight = pixWidth * 13 / 20;
        setPixmap (GraphRenderer::render (*m_graph, pixWidth, pixHeight, dpr));
        setFixedSize (pixWidth + kChrome, pixHeight + kChrome);
    }
};

class EquationLabel : public ClickableLabel {
    Q_OBJECT
private:
    QString m_latex;
    int m_equationNumber;
    int m_lastWidth = -1;
    qreal m_lastDpr = 0.0;
    static constexpr int kChrome = 2 * (8 + 1);

public:
    explicit EquationLabel (const QString &latex, int eqNum, QWidget *parent = nullptr)
        : ClickableLabel ("", parent), m_latex (latex), m_equationNumber (eqNum)
    {
        setAlignment (Qt::AlignCenter);
        setStyleSheet ("QLabel { padding: 8px; background-color: #FAFAFA; border: 1px solid #E0E0E0; border-radius: 4px; }");
    }

    void setBoxWidth (int boxWidth) {
        int pixWidth = qMax (200, boxWidth - kChrome);
        qreal dpr = devicePixelRatioF();
        if (pixWidth == m_lastWidth && dpr == m_lastDpr)
            return;
        m_lastWidth = pixWidth;
        m_lastDpr = dpr;

        int pixHeight = EquationRenderer::preferredHeight (m_latex, pixWidth, m_equationNumber);
        setPixmap (EquationRenderer::render (m_latex, pixWidth, pixHeight, dpr, m_equationNumber));
        setFixedSize (pixWidth + kChrome, pixHeight + kChrome);
    }
};

enum class WizardState {
    Idle,
    WaitingForGraphType,
    WaitingForGraphSource,
    WaitingForGraphXName,
    WaitingForGraphXValues,
    WaitingForGraphYName,
    WaitingForGraphYValues,
    WaitingForGraphXScale,
    WaitingForGraphYScale,
    WaitingForGraphDescription,
    WaitingForExportFormat
};

class EsbozoWindow : public QMainWindow {
    Q_OBJECT

private:
    Document m_document;
    CommandProcessor m_processor;
    size_t m_activeBlockIndex;
    QString m_currentFilePath;

    WizardState m_wizardState = WizardState::Idle;
    GraphType m_tempGraphType = GraphType::Line;
    GraphSource m_tempGraphSource = GraphSource::Manual;
    QString m_tempXName, m_tempYName;
    QVector<double> m_tempXVals, m_tempYVals;
    QVector<DataSeries> m_tempYSeries;
    AxisScale m_tempXScale = AxisScale::Linear;
    AxisScale m_tempYScale = AxisScale::Linear;

    QWidget *m_containerWidget;
    QVBoxLayout *m_documentLayout;
    QScrollArea *m_scrollArea;
    QLabel *m_statusLabel;
    QTextEdit *m_activeTextEdit = nullptr;

public:
    EsbozoWindow() : m_processor (m_document), m_activeBlockIndex (0) {
        updateWindowTitle();
        resize (800, 600);

        if (m_document.getBlockCount() == 0) {
            m_document.addBlock (std::make_unique<TitleBlock>());
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
                if (m_wizardState != WizardState::Idle) {
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
        return QMainWindow::eventFilter (obj, event);
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

    void resizeEvent (QResizeEvent *event) override {
        QMainWindow::resizeEvent (event);
        updateImageWidths();
        centerActiveBlock();
    }

private slots:
    void newProject() {
        saveCurrentContent();
        m_document = Document();
        m_document.addBlock (std::make_unique<TitleBlock>());
        m_activeBlockIndex = 0;
        m_currentFilePath.clear();
        m_wizardState = WizardState::Idle;
        updateWindowTitle();
        m_statusLabel->setText ("Nuevo proyecto creado");
        refreshUI();
    }

    void saveProject() {
        if (m_currentFilePath.isEmpty()) {
            QString filePath = QFileDialog::getSaveFileName (this, "Guardar Proyecto", "", "Archivos Esbozo (*.sbz)");
            if (filePath.isEmpty())
                return;
            if (!filePath.endsWith (".sbz", Qt::CaseInsensitive)) {
                filePath += ".sbz";
            }
            m_currentFilePath = filePath;
        }

        QJsonObject rootObj;
        rootObj["sbz_version"] = "1.0";

        QJsonArray blocksArray;
        size_t count = m_document.getBlockCount();
        for (size_t i = 0; i < count; ++i) {
            const Block* block = m_document.getBlock (i);
            if (!block)
                continue;

            QJsonObject blockObj;
            BlockType type = block->getType();

            if (type == BlockType::Title)
                blockObj["type"] = "Title";
            else if (type == BlockType::Subtitle)
                blockObj["type"] = "Subtitle";
            else if (type == BlockType::Paragraph)
                blockObj["type"] = "Paragraph";
            else if (type == BlockType::Image)
                blockObj["type"] = "Image";
            else if (type == BlockType::Graph)
                blockObj["type"] = "Graph";
            else if (type == BlockType::Equation)
                blockObj["type"] = "Equation";

            blockObj["content"] = block->getContent();

            if (type == BlockType::Graph) {
                if (const GraphBlock* gBlock = dynamic_cast<const GraphBlock*> (block)) {
                    blockObj["graphType"] = (gBlock->getGraphType() == GraphType::Line) ? "Line" : "Scatter";
                    blockObj["graphSource"] = (gBlock->getGraphSource() == GraphSource::Csv) ? "Csv" : "Manual";
                    blockObj["scaleX"] = (gBlock->getScaleX() == AxisScale::Log) ? "Log" : "Linear";
                    blockObj["scaleY"] = (gBlock->getScaleY() == AxisScale::Log) ? "Log" : "Linear";
                    blockObj["description"] = gBlock->getDescription();
                    blockObj["csvPath"] = gBlock->getCsvPath();

                    QJsonObject xObj;
                    xObj["name"] = gBlock->getSeriesX().name;
                    QJsonArray xValsArr;
                    for (double val : gBlock->getSeriesX().values)
                        xValsArr.append (val);
                    xObj["values"] = xValsArr;
                    blockObj["seriesX"] = xObj;

                    QJsonArray ySeriesArr;
                    for (const DataSeries& ds : gBlock->getSeriesYList()) {
                        QJsonObject dsObj;
                        dsObj["name"] = ds.name;
                        QJsonArray yValsArr;
                        for (double val : ds.values)
                            yValsArr.append (val);
                        dsObj["values"] = yValsArr;
                        ySeriesArr.append (dsObj);
                    }
                    blockObj["seriesYList"] = ySeriesArr;
                }
            }
            blocksArray.append (blockObj);
        }
        rootObj["blocks"] = blocksArray;

        QJsonDocument doc (rootObj);
        QFile file (m_currentFilePath);
        if (file.open (QIODevice::WriteOnly)) {
            file.write (doc.toJson());
            file.close();
            updateWindowTitle();
            m_statusLabel->setText ("Proyecto guardado exitosamente en: " + m_currentFilePath);
        } else {
            m_statusLabel->setText ("Error al guardar el archivo");
        }
    }

    void openProject() {
        QString filePath = QFileDialog::getOpenFileName (this, "Abrir Proyecto", "", "Archivos Esbozo (*.sbz)");
        if (filePath.isEmpty())
            return;

        QFile file (filePath);
        if (!file.open (QIODevice::ReadOnly)) {
            m_statusLabel->setText ("Error al abrir el archivo");
            return;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson (data);
        if (doc.isNull() || !doc.isObject()) {
            m_statusLabel->setText ("El archivo no es válido");
            return;
        }

        QJsonObject rootObj = doc.object();
        QJsonArray blocksArray = rootObj["blocks"].toArray();

        m_document = Document();
        m_activeBlockIndex = 0;

        for (const QJsonValue& val : blocksArray) {
            QJsonObject blockObj = val.toObject();
            QString typeStr = blockObj["type"].toString();
            QString content = blockObj["content"].toString();

            if (typeStr == "Title") {
                auto b = std::make_unique<TitleBlock>();
                b->setContent (content);
                m_document.addBlock (std::move (b));
            } else if (typeStr == "Subtitle") {
                auto b = std::make_unique<SubtitleBlock>();
                b->setContent (content);
                m_document.addBlock (std::move (b));
            } else if (typeStr == "Paragraph") {
                auto b = std::make_unique<ParagraphBlock>();
                b->setContent (content);
                m_document.addBlock (std::move (b));
            } else if (typeStr == "Image") {
                auto b = std::make_unique<ImageBlock>();
                b->setContent (content);
                m_document.addBlock (std::move (b));
            } else if (typeStr == "Graph") {
                auto b = std::make_unique<GraphBlock>();
                b->setContent (content);
                b->setGraphType (blockObj["graphType"].toString() == "Scatter" ? GraphType::Scatter : GraphType::Line);
                b->setGraphSource (blockObj["graphSource"].toString() == "Csv" ? GraphSource::Csv : GraphSource::Manual);
                b->setScales(
                    blockObj["scaleX"].toString() == "Log" ? AxisScale::Log : AxisScale::Linear,
                    blockObj["scaleY"].toString() == "Log" ? AxisScale::Log : AxisScale::Linear
                );
                b->setDescription (blockObj["description"].toString());
                b->setCsvPath (blockObj["csvPath"].toString());

                QJsonObject xObj = blockObj["seriesX"].toObject();
                QString xName = xObj["name"].toString();
                QVector<double> xVals;
                for (const QJsonValue& xv : xObj["values"].toArray()) xVals.append (xv.toDouble());
                b->setSeriesX (xName, xVals);

                QVector<DataSeries> ySeriesList;
                for (const QJsonValue& yvObj : blockObj["seriesYList"].toArray()) {
                    QJsonObject dsObj = yvObj.toObject();
                    DataSeries ds;
                    ds.name = dsObj["name"].toString();
                    for (const QJsonValue& yv : dsObj["values"].toArray()) ds.values.append (yv.toDouble());
                    ySeriesList.append (ds);
                }
                b->setSeriesYList (ySeriesList);

                m_document.addBlock (std::move (b));
            } else if (typeStr == "Equation") {
                auto b = std::make_unique<EquationBlock>();
                b->setContent (content);
                m_document.addBlock (std::move (b));
            }
        }

        if (m_document.getBlockCount() == 0) {
            m_document.addBlock (std::make_unique<TitleBlock>());
        }

        m_currentFilePath = filePath;
        updateWindowTitle();
        m_statusLabel->setText ("Proyecto cargado exitosamente desde: " + filePath);
        refreshUI();
    }

    void updateWindowTitle() {
        QString title = "Esbozo";
        if (!m_currentFilePath.isEmpty()) {
            title += " - " + QFileInfo (m_currentFilePath).fileName();
        } else {
            title += " - [Nuevo Proyecto]";
        }
        setWindowTitle (title);
    }

    void startExportWizard() {
        if (navigationLocked()) {
            showLockedMessage();
            return;
        }
        saveCurrentContent();
        m_wizardState = WizardState::WaitingForExportFormat;
        updateWizardPrompt ("md (Markdown) o pdf (PDF)");
    }

    void handleInputSubmitted() {
        if (!m_activeTextEdit)
            return;

        QString text = m_activeTextEdit->toPlainText();

        if (m_wizardState != WizardState::Idle) {
            QString trimmedText = text.trimmed();

            if (trimmedText == "can;;") {
                m_wizardState = WizardState::Idle;
                refreshUI();
                return;
            }

            if (m_wizardState == WizardState::WaitingForExportFormat) {
                if (trimmedText == "md") {
                    m_wizardState = WizardState::Idle;
                    refreshUI();
                    exportToMarkdown();
                } else if (trimmedText == "pdf") {
                    m_wizardState = WizardState::Idle;
                    m_statusLabel->setText ("Exportación a PDF cancelada (no disponible por el momento)");
                    refreshUI();
                } else {
                    m_activeTextEdit->clear();
                }
                return;
            }

            switch (m_wizardState) {
                case WizardState::WaitingForGraphType:
                    if (trimmedText == "lin")
                        m_tempGraphType = GraphType::Line;
                    else if (trimmedText == "sct") 
                        m_tempGraphType = GraphType::Scatter;
                    else {
                        m_activeTextEdit->clear();
                        return;
                    }
                    
                    m_wizardState = WizardState::WaitingForGraphSource;
                    updateWizardPrompt ("csv (archivo) o man (entrada manual)");
                    break;
                
                case WizardState::WaitingForGraphSource:
                    if (trimmedText == "csv") {
                        m_tempGraphSource = GraphSource::Csv;
                        openCsvFileDialog();
                    } else if (trimmedText == "man") {
                        m_tempGraphSource = GraphSource::Manual;
                        m_wizardState = WizardState::WaitingForGraphXName;
                        updateWizardPrompt ("Nombre de la serie X");
                    } else {
                        m_activeTextEdit->clear();
                        return;
                    }
                    break;

                case WizardState::WaitingForGraphXName:
                    if (trimmedText.isEmpty()) {
                        m_activeTextEdit->clear();
                        return;
                    }
                    m_tempXName = trimmedText;
                    m_wizardState = WizardState::WaitingForGraphXValues;
                    updateWizardPrompt ("Valores de X separados por comas");
                    break;

                case WizardState::WaitingForGraphXValues:
                    m_tempXVals = parseCommaValues (trimmedText);
                    if (m_tempXVals.isEmpty()) {
                        m_activeTextEdit->clear();
                        return;
                    }
                    m_wizardState = WizardState::WaitingForGraphYName;
                    updateWizardPrompt ("Nombre de la serie Y");
                    break;

                case WizardState::WaitingForGraphYName:
                    if (trimmedText.isEmpty()) {
                        m_activeTextEdit->clear();
                        return;
                    }
                    m_tempYName = trimmedText;
                    m_wizardState = WizardState::WaitingForGraphYValues;
                    updateWizardPrompt ("Valores de Y separados por comas");
                    break;

                case WizardState::WaitingForGraphYValues:
                    m_tempYVals = parseCommaValues (trimmedText);
                    if (m_tempYVals.isEmpty() || m_tempYVals.size() != m_tempXVals.size()) {
                        m_activeTextEdit->clear();
                        return;
                    }
                    m_tempYSeries = { DataSeries{ m_tempYName, m_tempYVals } };
                    m_wizardState = WizardState::WaitingForGraphXScale;
                    updateWizardPrompt ("lin (eje x lineal) o log (eje x logarítmico)");
                    break;

                case WizardState::WaitingForGraphXScale:
                    if (trimmedText == "lin")
                        m_tempXScale = AxisScale::Linear;
                    else if (trimmedText == "log")
                        m_tempXScale = AxisScale::Log;
                    else {
                        m_activeTextEdit->clear();
                        return;
                    }

                    m_wizardState = WizardState::WaitingForGraphYScale;
                    updateWizardPrompt ("lin (eje y lineal) o log (eje y logarítmico)");
                    break;
                
                case WizardState::WaitingForGraphYScale:
                    if (trimmedText == "lin")
                        m_tempYScale = AxisScale::Linear;
                    else if (trimmedText == "log")
                        m_tempYScale = AxisScale::Log;
                    else {
                        m_activeTextEdit->clear();
                        return;
                    }

                    m_wizardState = WizardState::WaitingForGraphDescription;
                    updateWizardPrompt ("descripción de la figura");
                    break;

                case WizardState::WaitingForGraphDescription:
                    {
                        if (auto* gBlock = dynamic_cast<GraphBlock*> (m_document.getBlock (m_activeBlockIndex))) {
                            gBlock->setGraphType (m_tempGraphType);
                            gBlock->setGraphSource (m_tempGraphSource);
                            gBlock->setSeriesX (m_tempXName, m_tempXVals);
                            gBlock->setSeriesYList (m_tempYSeries);
                            gBlock->setScales (m_tempXScale, m_tempYScale);
                            gBlock->setDescription (trimmedText);
                        }
                        m_wizardState = WizardState::Idle;
                        refreshUI();
                    }
                    break;
                
                default:
                    break;
            }
            return;
        }
        
        m_processor.setActiveBlockIndex (m_activeBlockIndex);
        CommandResult result = m_processor.processInput (inputForProcessor());

        if (result == CommandResult::BlockCreated || result == CommandResult::SectionFinished || result == CommandResult::BlockDeleted) {
            m_activeBlockIndex = m_processor.getActiveBlockIndex();
        }

        refreshUI();

        if (Block* currentBlock = m_document.getBlock (m_activeBlockIndex)) {
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
            else if (currentBlock->getType() == BlockType::Graph && m_wizardState == WizardState::Idle) {
                m_wizardState = WizardState::WaitingForGraphType;
                updateWizardPrompt ("lin (línea) o sct (dispersión)");
            }
        }
    }

private:
    void exportToMarkdown() {
        QString filePath = QFileDialog::getSaveFileName (this, "Exportar a Markdown", "", "Archivos Markdown (*.md)");
        if (filePath.isEmpty())
            return;

        if (!filePath.endsWith (".md", Qt::CaseInsensitive)) {
            filePath += ".md";
        }

        QFileInfo fileInfo (filePath);
        QDir exportDir = fileInfo.dir();
        QString imagesDirName = "images";
        QDir imagesDir (exportDir.filePath (imagesDirName));

        if (!imagesDir.exists()) {
            exportDir.mkdir (imagesDirName);
        }

        QString mdContent;
        size_t count = m_document.getBlockCount();
        int graphCounter = 1;

        for (size_t i = 0; i < count; ++i) {
            const Block* block = m_document.getBlock (i);
            if (!block)
                continue;

            BlockType type = block->getType();

            if (type == BlockType::Title) {
                mdContent += "# " + block->getContent() + "\n\n";
            }
            else if (type == BlockType::Subtitle) {
                mdContent += "### " + block->getContent() + "\n\n";
            }
            else if (type == BlockType::Paragraph) {
                mdContent += block->getContent() + "\n\n";
            }
            else if (type == BlockType::Image) {
                QString srcPath = block->getContent();
                if (!srcPath.isEmpty() && QFile::exists (srcPath)) {
                    QFileInfo imgInfo (srcPath);
                    QString destFileName = imgInfo.fileName();
                    QString destPath = imagesDir.filePath (destFileName);

                    QFile::copy (srcPath, destPath);
                    mdContent += QString ("![Imagen](%1/%2)\n").arg (imagesDirName, destFileName);
                    mdContent += QString ("> *%1*\n\n").arg (destFileName);
                } else {
                    mdContent += "![Imagen](imagen_no_encontrada)\n";
                    mdContent += "> *imagen_no_encontrada*\n\n";
                }
            }
            else if (type == BlockType::Graph) {
                if (const GraphBlock* gBlock = dynamic_cast<const GraphBlock*> (block)) {
                    QString graphFileName = QString ("grafico_%1.png").arg (graphCounter++);
                    QString destPath = imagesDir.filePath (graphFileName);

                    QPixmap pixmap = GraphRenderer::render (*gBlock, 800, 520, 2.0);
                    pixmap.save (destPath, "PNG");

                    mdContent += QString ("![Gráfico](%1/%2)\n").arg (imagesDirName, graphFileName);
                }
            } else if (type == BlockType::Equation) {
                mdContent += "$$\n" + block->getContent() + "\n$$\n\n";
            }
        }

        QFile outFile (filePath);
        if (outFile.open (QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out (&outFile);
            out << mdContent;
            outFile.close();
            m_statusLabel->setText ("Documento exportado exitosamente a: " + filePath);
        } else {
            m_statusLabel->setText ("Error al exportar el archivo Markdown");
        }
    }

    void discardActiveBlock() {
        if (m_document.getBlockCount() > 1) {
            size_t indexToDelete = m_activeBlockIndex;
            if (m_activeBlockIndex > 0) m_activeBlockIndex--;
            m_document.removeBlock (indexToDelete);
        } else if (Block* b = m_document.getBlock (m_activeBlockIndex)) {
            b->setContent ("");
        }
        refreshUI();
    }

    bool navigationLocked() const {
        return m_wizardState != WizardState::Idle;
    }

    void showLockedMessage() {
        QString previous = m_statusLabel->text();
        m_statusLabel->setText ("Terminá la operación actual o cancelala con 'can;;' antes de cambiar de bloque");
        QTimer::singleShot (2500, this, [this, previous]() {
            m_statusLabel->setText (previous);
        });
    }

    void activateBlock (size_t index) {
        if (navigationLocked()) {
            showLockedMessage();
            return;
        }
        saveCurrentContent();
        m_activeBlockIndex = index;
        refreshUI();
    }

    void updateWizardPrompt (const QString& promptText) {
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
        if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
            return false;

        QTextStream in (&file);
        if (in.atEnd())
            return false;

        const QRegularExpression sep ("[,;\\t]");
        const double nan = std::numeric_limits<double>::quiet_NaN();

        QStringList headers = in.readLine().split (sep);
        if (headers.size() < 2)
            return false;
        for (QString& h : headers)
            h = h.trimmed();

        m_tempXName = headers[0];
        m_tempXVals.clear();
        m_tempYSeries.clear();

        for (int c = 1; c < headers.size(); ++c) {
            DataSeries s;
            s.name = headers[c].isEmpty() ? QString ("Serie %1").arg (c) : headers[c];
            m_tempYSeries.append (s);
        }

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty())
                continue;

            QStringList parts = line.split (sep);
            bool okX = false;
            double x = parts[0].trimmed().toDouble (&okX);
            if (!okX)
                continue;

            m_tempXVals.append (x);
            for (int c = 0; c < m_tempYSeries.size(); ++c) {
                bool ok = false;
                double y = (c + 1 < parts.size()) ? parts[c + 1].trimmed().toDouble (&ok) : nan;
                m_tempYSeries[c].values.append (ok ? y : nan);
            }
        }
        file.close();

        for (int c = m_tempYSeries.size() - 1; c >= 0; --c) {
            bool any = false;
            for (double v : m_tempYSeries[c].values)
                if (std::isfinite (v)) {
                    any = true;
                    break;
                }
            if (!any)
                m_tempYSeries.removeAt (c);
        }

        return !m_tempXVals.isEmpty() && !m_tempYSeries.isEmpty();
    }

    void openCsvFileDialog() {
        QFileDialog dialog (this, "Seleccionar Archivo CSV");
        dialog.setOption (QFileDialog::DontUseNativeDialog, true);
        dialog.setFileMode (QFileDialog::ExistingFile);
        dialog.setNameFilter ("Archivos CSV (*.csv)");
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
                    m_wizardState = WizardState::WaitingForGraphXScale;
                    updateWizardPrompt (QString ("CSV cargado (%1 series Y). Escala eje X: 'lin' o 'log'").arg (m_tempYSeries.size()));
                } else {
                    updateWizardPrompt ("Error al leer el CSV. Escribe 'can;;' para cancelar o reintenta.");
                }
            }
        } else {
            m_wizardState = WizardState::Idle;
            discardActiveBlock();
        }
    }

    bool canFormat() const {
        if (!m_activeTextEdit || navigationLocked())
            return false;
        const Block* b = m_document.getBlock (m_activeBlockIndex);
        return b && InlineMarkup::isRichType (b->getType());
    }

    void toggleBold() {
        if (!canFormat()) return;
        QTextCharFormat fmt;
        bool on = m_activeTextEdit->currentCharFormat().fontWeight() >= QFont::Bold;
        fmt.setFontWeight (on ? QFont::Normal : QFont::Bold);
        m_activeTextEdit->mergeCurrentCharFormat (fmt);
    }

    void toggleItalic() {
        if (!canFormat()) return;
        QTextCharFormat fmt;
        fmt.setFontItalic (!m_activeTextEdit->currentCharFormat().fontItalic());
        m_activeTextEdit->mergeCurrentCharFormat (fmt);
    }

    QString inputForProcessor() {
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

    void updateStatus() {
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

    void saveCurrentContent() {
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

    void saveAndNavigate (int delta) {
        if (navigationLocked()) {
            showLockedMessage();
            return;
        }
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
            } else if (auto *gr = qobject_cast<GraphLabel*> (item->widget())) {
                gr->setBoxWidth (available * 3 / 4);
            } else if (auto *eq = qobject_cast<EquationLabel*> (item->widget())) {
                eq->setBoxWidth (available * 3 / 4);
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
                editBox->setAcceptRichText (false);

                if (InlineMarkup::isRichType (type))
                    InlineMarkup::load (editBox->document(), block->getContent());
                else
                    editBox->setPlainText (block->getContent());
                editBox->moveCursor (QTextCursor::End);

                if (type == BlockType::Title) {
                    editBox->setFont (QFont ("Segoe UI", 16));
                }
                else if (type == BlockType::Subtitle) {
                    editBox->setFont (QFont ("Segoe UI", 14));
                }
                else if (type == BlockType::Image) {
                    QFont f ("Segoe UI", 11);
                    f.setItalic (true);
                    editBox->setFont (f);
                    if (block->getContent().isEmpty()) {
                        editBox->setPlaceholderText ("Ruta de la imagen");
                    }
                }
                else if (type == BlockType::Equation) {
                    editBox->setFont (QFont ("Consolas", 12));
                    if (block->getContent().isEmpty())
                        editBox->setPlaceholderText ("ecuación en LaTeX");
                }
                else {
                    editBox->setFont (QFont ("Segoe UI", 13));
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
                        this, [updateHeight] (const QSizeF &) {
                            updateHeight();
                        }
                    );

                connect (editBox, &QTextEdit::textChanged, this, [this]() {
                    saveCurrentContent();
                });

                connect (editBox, &QTextEdit::currentCharFormatChanged, this, [this] (const QTextCharFormat &) {
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
            } else {
                if (type == BlockType::Image) {
                    QString path = block->getContent();
                    
                    ImageLabel *label = new ImageLabel (path, m_containerWidget);

                    connect (label, &ImageLabel::doubleClicked, this, [this, i]() { activateBlock (i); });

                    m_documentLayout->addWidget (label, 0, Qt::AlignCenter);
                } else if (type == BlockType::Graph) {
                    if (const GraphBlock* gBlock = dynamic_cast<const GraphBlock*> (block)) {
                        auto snapshot = std::make_shared<GraphBlock> (*gBlock);
                        GraphLabel *graphLabel = new GraphLabel (snapshot, m_containerWidget);

                        connect (graphLabel, &GraphLabel::doubleClicked, this, [this, i]() { activateBlock (i); });

                        m_documentLayout->addWidget (graphLabel, 0, Qt::AlignCenter);
                    }
                } else if (type == BlockType::Equation) {
                    QString content = block->getContent();
                    
                    int eqNum = 0;
                    for (size_t k = 0; k <= i; ++k) {
                        if (m_document.getBlock(k)->getType() == BlockType::Equation) {
                            eqNum++;
                        }
                    }

                    EquationLabel *eqLabel = new EquationLabel (content, eqNum, m_containerWidget);

                    connect (eqLabel, &EquationLabel::doubleClicked, this, [this, i]() { activateBlock (i); });

                    m_documentLayout->addWidget (eqLabel, 0, Qt::AlignCenter);
                } else {
                    QString content = block->getContent();
                    QString html = content.isEmpty() ? QString ("(vacío)") : InlineMarkup::toHtml (content);

                    ClickableLabel *label = new ClickableLabel (html, m_containerWidget);
                    label->setTextFormat (Qt::RichText);

                    if (type == BlockType::Title) {
                        label->setFont (QFont ("Segoe UI", 15));
                        label->setStyleSheet ("QLabel { color: #222222; padding: 6px; }");
                    } else if (type == BlockType::Subtitle) {
                        label->setFont (QFont ("Segoe UI", 13));
                        label->setStyleSheet ("QLabel { color: #444444; padding: 6px; }");
                    } else {
                        label->setFont (QFont ("Segoe UI", 11, QFont::Normal));
                        label->setStyleSheet ("QLabel { color: #555555; padding: 6px; }");
                    }

                    label->setWordWrap (true);
                    
                    connect (label, &ClickableLabel::doubleClicked, this, [this, i]() { activateBlock (i); });

                    m_documentLayout->addWidget (label);
                }
            }
        }

        updateImageWidths();

        updateStatus();
        
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