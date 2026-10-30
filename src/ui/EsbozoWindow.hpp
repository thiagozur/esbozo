#pragma once

#include <QMainWindow>
#include <QString>

#include "core/CommandProcessor.hpp"
#include "core/Document.hpp"
#include "ui/Wizard.hpp"

class QEvent;
class QKeyEvent;
class QLabel;
class QResizeEvent;
class QScrollArea;
class QTextEdit;
class QVBoxLayout;
class QWidget;

class EsbozoWindow : public QMainWindow, private WizardHost {
    Q_OBJECT

public:
    EsbozoWindow();

    bool loadProject (const QString& path);
    const Document& currentDocument() const { return m_document; }
    bool wizardActive() const { return m_wizard.active(); }

protected:
    bool eventFilter (QObject* obj, QEvent* event) override;
    void keyPressEvent (QKeyEvent* event) override;
    void resizeEvent (QResizeEvent* event) override;

private:
    Document& document() override { return m_document; }
    size_t activeIndex() const override { return m_activeBlockIndex; }
    void setPrompt (const QString& text) override;
    void clearInput() override;
    void refresh() override { refreshUI(); }
    void discardActiveBlock() override;
    void showMessage (const QString& text) override;
    QString pickFile (const QString& title, const QString& nameFilter) override;
    void exportAs (const QString& format) override;

    void newProject();
    void saveProject();
    void openProject();
    void updateWindowTitle();
    void cycleTheme();
    void startExportWizard();
    void exportToMarkdown();
    void exportToPdf();

    void handleInputSubmitted();
    bool navigationLocked() const;
    void showLockedMessage();
    void activateBlock (size_t index);
    void saveCurrentContent();
    void saveAndNavigate (int delta);

    bool canFormat() const;
    void toggleBold();
    void toggleItalic();
    QString inputForProcessor();
    void updateStatus();

    void updateImageWidths();
    void centerActiveBlock();
    void refreshUI();
    void addEditor (Block* block);
    void addInactiveWidget (size_t index, Block* block);

    Document m_document;
    CommandProcessor m_processor;
    size_t m_activeBlockIndex;
    QString m_currentFilePath;
    Wizard m_wizard;

    QWidget* m_containerWidget = nullptr;
    QVBoxLayout* m_documentLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTextEdit* m_activeTextEdit = nullptr;
};
