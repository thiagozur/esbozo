#include "ui/BlockWidgets.hpp"

#include "core/EquationRenderer.hpp"
#include "core/GraphRenderer.hpp"
#include "core/TableRenderer.hpp"
#include "ui/Theme.hpp"

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

    QPixmap scaled = m_original.scaledToWidth (pixWidth, Qt::SmoothTransformation);
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
    setPixmap (GraphRenderer::render (*m_graph, pixWidth, pixHeight, dpr));
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
    setPixmap (EquationRenderer::render (m_latex, pixWidth, pixHeight, dpr, m_equationNumber));
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
    setPixmap (TableRenderer::render (m_data, m_hasHeaderRow, m_hasRowNames, m_title, pixWidth, pixHeight, dpr));
    setFixedSize (pixWidth + chrome, pixHeight + chrome);
}
