#pragma once

#include <QColor>
#include <QLabel>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QVector>
#include <memory>

#include "core/Block.hpp"
#include "core/GraphBlock.hpp"

class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel (const QString& text, QWidget* parent = nullptr);

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent (QMouseEvent* event) override;
};

class ImageLabel : public ClickableLabel {
    Q_OBJECT

public:
    explicit ImageLabel (const QString& path, QWidget* parent = nullptr);
    void setBoxWidth (int boxWidth);

private:
    QPixmap m_original;
    int m_lastWidth = -1;
};

class GraphLabel : public ClickableLabel {
    Q_OBJECT

public:
    explicit GraphLabel (std::shared_ptr<const GraphBlock> graph, QWidget* parent = nullptr);
    void setBoxWidth (int boxWidth);

private:
    std::shared_ptr<const GraphBlock> m_graph;
    int m_lastWidth = -1;
    qreal m_lastDpr = 0.0;
};

class EquationLabel : public ClickableLabel {
    Q_OBJECT

public:
    EquationLabel (const QString& latex, int equationNumber, QWidget* parent = nullptr);
    void setBoxWidth (int boxWidth);

private:
    QString m_latex;
    int m_equationNumber;
    int m_lastWidth = -1;
    qreal m_lastDpr = 0.0;
};

class TableLabel : public ClickableLabel {
    Q_OBJECT

public:
    TableLabel (const QVector<QStringList>& data, const QString& title, bool hasHeader, bool hasRowNames, QWidget* parent = nullptr);
    void setBoxWidth (int boxWidth);

private:
    QVector<QStringList> m_data;
    QString m_title;
    bool m_hasHeaderRow;
    bool m_hasRowNames;
    int m_lastWidth = -1;
    qreal m_lastDpr = 0.0;
};

QString commandLabelFor (BlockType type);

class FrameOverlay;

class BlockEditor : public QTextEdit {
    Q_OBJECT

public:
    explicit BlockEditor (QWidget* parent = nullptr);
    void setLabelText (const QString& text);
    void setWizardMode (bool wizard);
    void setFrameVisible (bool visible);

protected:
    void paintEvent (QPaintEvent* event) override;
    void resizeEvent (QResizeEvent* event) override;
    void focusInEvent (QFocusEvent* event) override;
    void focusOutEvent (QFocusEvent* event) override;

private:
    void restartBlink();
    void applyColors();

    QColor m_caretColor;
    bool m_wizard = false;
    bool m_caretVisible = true;
    QTimer m_blinkTimer;
    FrameOverlay* m_frame = nullptr;
};

class BlockView : public QWidget {
    Q_OBJECT

public:
    BlockView (Block* block, QWidget* parent = nullptr);

    Block* block() const { return m_block; }
    BlockType type() const { return m_type; }
    BlockEditor* editor() const { return m_editor; }
    bool isTextual() const;

    void syncFromBlock();
    void setFocusedLook (bool focused, bool force = false);
    void setEquationNumber (int number);
    void setCardWidths (int imageBox, int wideBox);

signals:
    void activated();
    void contentHeightChanged();

protected:
    void mouseDoubleClickEvent (QMouseEvent* event) override;

private:
    void updateEditorHeight();
    void rebuildCard();
    void applyCardWidth();
    QString fingerprint() const;

    Block* m_block;
    BlockType m_type;
    BlockEditor* m_editor = nullptr;
    ClickableLabel* m_card = nullptr;
    QString m_cardKey;
    int m_equationNumber = 0;
    int m_imageBox = 0;
    int m_wideBox = 0;
    bool m_focused = false;
    bool m_styled = false;
};
