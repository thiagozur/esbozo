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

class BlockEditor : public QTextEdit {
    Q_OBJECT

public:
    explicit BlockEditor (QWidget* parent = nullptr);
    void setCaretColor (const QColor& color);

protected:
    void paintEvent (QPaintEvent* event) override;
    void focusInEvent (QFocusEvent* event) override;
    void focusOutEvent (QFocusEvent* event) override;

private:
    void restartBlink();

    QColor m_caretColor;
    bool m_caretVisible = true;
    QTimer m_blinkTimer;
};
