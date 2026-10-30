#include "ui/BlockWidgets.hpp"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include "core/EquationRenderer.hpp"
#include "core/GraphRenderer.hpp"
#include "core/TableRenderer.hpp"
#include "ui/Theme.hpp"

namespace {

GraphRenderer::Colors graphColors() {
    const Theme::Colors& c = Theme::colors();
    GraphRenderer::Colors g;
    g.title = c.text;
    g.label = c.text;
    g.tick = Theme::withAlpha (c.text, 0.85);
    g.frame = Theme::withAlpha (c.text, 0.60);
    g.grid = Theme::withAlpha (c.text, 0.10);
    g.empty = Theme::withAlpha (c.text, 0.50);
    g.background = Qt::transparent;
    g.markerRing = Qt::transparent;
    return g;
}

TableRenderer::Colors tableColors() {
    const Theme::Colors& c = Theme::colors();
    TableRenderer::Colors t;
    t.cell = Qt::transparent;
    t.header = Qt::transparent;
    t.grid = Theme::withAlpha (c.text, 0.25);
    t.headerRule = Theme::withAlpha (c.text, 0.55);
    t.headerText = c.text;
    t.bodyText = Theme::withAlpha (c.text, 0.90);
    t.title = c.text;
    t.empty = Theme::withAlpha (c.text, 0.50);
    return t;
}

QPixmap roundedCorners (const QPixmap& source, int radius) {
    QPixmap out (source.size());
    out.fill (Qt::transparent);
    QPainter painter (&out);
    painter.setRenderHint (QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect (QRectF (QPointF (0, 0), QSizeF (source.size())), radius, radius);
    painter.setClipPath (path);
    painter.drawPixmap (0, 0, source);
    return out;
}

}

ClickableLabel::ClickableLabel (const QString& text, QWidget* parent)
    : QLabel (text, parent)
{
}

void ClickableLabel::mouseDoubleClickEvent (QMouseEvent* event) {
    QLabel::mouseDoubleClickEvent (event);
    emit doubleClicked();
}

ImageLabel::ImageLabel (const QString& path, QWidget* parent)
    : ClickableLabel ("", parent), m_original (path)
{
    setAlignment (Qt::AlignCenter);

    if (path.isEmpty()) {
        setText ("(imagen vacía)");
        setStyleSheet (Theme::mutedLabelStyle());
    } else if (m_original.isNull()) {
        setText ("[Error cargando imagen: " + path + "]");
        setStyleSheet (Theme::errorLabelStyle());
    } else {
        setStyleSheet (Theme::cardStyle());
    }
}

void ImageLabel::setBoxWidth (int boxWidth) {
    if (m_original.isNull())
        return;

    const int chrome = Theme::metrics().cardChrome();
    int pixWidth = qMax (50, boxWidth - chrome);
    if (pixWidth == m_lastWidth)
        return;
    m_lastWidth = pixWidth;

    QPixmap scaled = roundedCorners (m_original.scaledToWidth (pixWidth, Qt::SmoothTransformation), Theme::metrics().imageRadius);
    setPixmap (scaled);
    setFixedSize (scaled.width() + chrome, scaled.height() + chrome);
}

GraphLabel::GraphLabel (std::shared_ptr<const GraphBlock> graph, QWidget* parent)
    : ClickableLabel ("", parent), m_graph (std::move (graph))
{
    setAlignment (Qt::AlignCenter);
    setStyleSheet (Theme::cardStyle());
}

void GraphLabel::setBoxWidth (int boxWidth) {
    const int chrome = Theme::metrics().cardChrome();
    int pixWidth = qMax (260, boxWidth - chrome);
    qreal dpr = devicePixelRatioF();
    if (pixWidth == m_lastWidth && dpr == m_lastDpr)
        return;
    m_lastWidth = pixWidth;
    m_lastDpr = dpr;

    int pixHeight = pixWidth * 13 / 20;
    setPixmap (GraphRenderer::render (*m_graph, pixWidth, pixHeight, dpr, graphColors()));
    setFixedSize (pixWidth + chrome, pixHeight + chrome);
}

EquationLabel::EquationLabel (const QString& latex, int equationNumber, QWidget* parent)
    : ClickableLabel ("", parent), m_latex (latex), m_equationNumber (equationNumber)
{
    setAlignment (Qt::AlignCenter);
    setStyleSheet (Theme::cardStyle());
}

void EquationLabel::setBoxWidth (int boxWidth) {
    const int chrome = Theme::metrics().cardChrome();
    int pixWidth = qMax (200, boxWidth - chrome);
    qreal dpr = devicePixelRatioF();
    if (pixWidth == m_lastWidth && dpr == m_lastDpr)
        return;
    m_lastWidth = pixWidth;
    m_lastDpr = dpr;

    int pixHeight = EquationRenderer::preferredHeight (m_latex, pixWidth, m_equationNumber);
    setPixmap (EquationRenderer::render (m_latex, pixWidth, pixHeight, dpr, m_equationNumber, Theme::colors().text));
    setFixedSize (pixWidth + chrome, pixHeight + chrome);
}

TableLabel::TableLabel (const QVector<QStringList>& data, const QString& title, bool hasHeader, bool hasRowNames, QWidget* parent)
    : ClickableLabel ("", parent), m_data (data), m_title (title), m_hasHeaderRow (hasHeader), m_hasRowNames (hasRowNames)
{
    setAlignment (Qt::AlignCenter);
    setStyleSheet (Theme::cardStyle());
}

void TableLabel::setBoxWidth (int boxWidth) {
    const int chrome = Theme::metrics().cardChrome();
    int pixWidth = qMax (200, boxWidth - chrome);
    qreal dpr = devicePixelRatioF();
    if (pixWidth == m_lastWidth && dpr == m_lastDpr)
        return;
    m_lastWidth = pixWidth;
    m_lastDpr = dpr;

    int pixHeight = TableRenderer::preferredHeight (m_data, m_title);
    setPixmap (TableRenderer::render (m_data, m_hasHeaderRow, m_hasRowNames, m_title, pixWidth, pixHeight, dpr, tableColors()));
    setFixedSize (pixWidth + chrome, pixHeight + chrome);
}

BlockEditor::BlockEditor (QWidget* parent)
    : QTextEdit (parent), m_caretColor (Theme::colors().accent)
{
    setCursorWidth (0);

    const int flash = QApplication::cursorFlashTime();
    if (flash > 0) {
        m_blinkTimer.setInterval (flash / 2);
        connect (&m_blinkTimer, &QTimer::timeout, this, [this]() {
            m_caretVisible = !m_caretVisible;
            viewport()->update();
        });
    }

    connect (this, &QTextEdit::cursorPositionChanged, this, &BlockEditor::restartBlink);
    connect (this, &QTextEdit::textChanged, this, &BlockEditor::restartBlink);
}

void BlockEditor::setCaretColor (const QColor& color) {
    m_caretColor = color;
    viewport()->update();
}

void BlockEditor::restartBlink() {
    m_caretVisible = true;
    if (m_blinkTimer.interval() > 0 && hasFocus())
        m_blinkTimer.start();
    viewport()->update();
}

void BlockEditor::focusInEvent (QFocusEvent* event) {
    QTextEdit::focusInEvent (event);
    restartBlink();
}

void BlockEditor::focusOutEvent (QFocusEvent* event) {
    QTextEdit::focusOutEvent (event);
    m_blinkTimer.stop();
    viewport()->update();
}

void BlockEditor::paintEvent (QPaintEvent* event) {
    QTextEdit::paintEvent (event);

    if (!hasFocus() || !m_caretVisible || isReadOnly() || textCursor().hasSelection())
        return;

    QPainter painter (viewport());
    const QRect r = cursorRect();
    painter.fillRect (QRect (r.left(), r.top(), Theme::metrics().caretWidth, r.height()), m_caretColor);
}
