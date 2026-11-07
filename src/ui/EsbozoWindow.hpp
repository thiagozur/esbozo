#pragma once

#include <QMainWindow>
#include <QString>
#include <vector>

#include "core/CommandProcessor.hpp"
#include "core/Document.hpp"
#include "ui/Wizard.hpp"

class BlockEditor;
class BlockView;
class EdgeFade;
namespace Backdrop { class Widget; }
class QVariantAnimation;
class QEvent;
class QKeyEvent;
class QLabel;
class QResizeEvent;
class QScrollArea;
class QVBoxLayout;
class QWidget;

class EsbozoWindow : public QMainWindow, private WizardHost {
    Q_OBJECT

public:
    EsbozoWindow();

    bool loadProject (const QString& path);
    const Document& currentDocument() const { return m_document; }
    bool wizardActive() const { return m_wizard.active(); }
    BlockEditor* activeEditor() const { return m_activeTextEdit; }
    void setAnimationsEnabled (bool enabled);
    int verticalMargin() const;

protected:
    bool eventFilter (QObject* obj, QEvent* event) override;
    void keyPressEvent (QKeyEvent* event) override;
    void resizeEvent (QResizeEvent* event) override;

private:
    Document& document() override { return m_document; }
    size_t activeIndex() const override { return m_activeBlockIndex; }
    void setPrompt (const QString& text) override;
    void clearInput() override;
    void refresh() override { refreshUI (true); }
    void discardActiveBlock() override;
    void showMessage (const QString& text) override;
    QString pickFile (const QString& title, const QString& nameFilter) override;
    void exportAs (const QString& format) override;

    void newProject();
    void saveProject();
    void openProject();
    void updateWindowTitle();
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

    int sideMargin() const;
    void updateImageWidths();
    void centerActiveBlock();
    void refreshUI (bool animate = true);
    void startTransition (BlockView* from, BlockView* to);
    void applyTransition (qreal progress);
    void finishTransition (bool recenter);
    BlockView* createView (Block* block);
    int indexOfView (const BlockView* view) const;

    Document m_document;
    CommandProcessor m_processor;
    size_t m_activeBlockIndex;
    QString m_currentFilePath;
    Wizard m_wizard;

    QWidget* m_containerWidget = nullptr;
    QVBoxLayout* m_documentLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_statusLabel = nullptr;
    BlockEditor* m_activeTextEdit = nullptr;
    std::vector<BlockView*> m_views;
    bool m_syncing = false;

    QVariantAnimation* m_transition = nullptr;
    BlockView* m_activeView = nullptr;
    BlockView* m_fadeIn = nullptr;
    BlockView* m_fadeOut = nullptr;
    EdgeFade* m_fade = nullptr;
    Backdrop::Widget* m_backdrop = nullptr;
    qreal m_progress = 1.0;
    int m_scrollStart = 0;
    bool m_transitioning = false;
    bool m_centering = false;
    bool m_animationsEnabled = true;
};
