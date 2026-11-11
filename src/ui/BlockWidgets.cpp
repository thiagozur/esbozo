#include "ui/BlockWidgets.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QVBoxLayout>

#include "core/EquationRenderer.hpp"
#include "core/GraphRenderer.hpp"
#include "core/InlineMarkup.hpp"
#include "core/TableBlock.hpp"
#include "core/TableRenderer.hpp"
#include "ui/Theme.hpp"

namespace {

GraphRenderer::Style graphStyle() {
    const Theme::Colors& c = Theme::colors();
    GraphRenderer::Style g;
    g.fontFamily = Theme::fonts().family;
    g.fontScale = Theme::sizes().contentScale;
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

TableRenderer::Style tableStyle() {
    const Theme::Colors& c = Theme::colors();
    TableRenderer::Style t;
    t.fontFamily = Theme::fonts().family;
    t.fontScale = Theme::sizes().contentScale;
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
    setPixmap (GraphRenderer::render (*m_graph, pixWidth, pixHeight, dpr, graphStyle()));
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

    int pixHeight = EquationRenderer::preferredHeight (m_latex, pixWidth, m_equationNumber, Theme::sizes().contentScale);
    setPixmap (EquationRenderer::render (m_latex, pixWidth, pixHeight, dpr, m_equationNumber, Theme::colors().text, Theme::sizes().contentScale));
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

    int pixHeight = TableRenderer::preferredHeight (m_data, m_title, tableStyle());
    setPixmap (TableRenderer::render (m_data, m_hasHeaderRow, m_hasRowNames, m_title, pixWidth, pixHeight, dpr, tableStyle()));
    setFixedSize (pixWidth + chrome, pixHeight + chrome);
}

QString commandLabelFor (BlockType type) {
    switch (type) {
        case BlockType::Title: return "ttl;;";
        case BlockType::Subtitle: return "sttl;;";
        case BlockType::Paragraph: return "par;;";
        case BlockType::Image: return "ft;;";
        case BlockType::Graph: return "grf;;";
        case BlockType::Equation: return "eq;;";
        case BlockType::Table: return "tbl;;";
    }
    return QString();
}

class FrameOverlay : public QWidget {
public:
    explicit FrameOverlay (QWidget* parent)
        : QWidget (parent)
    {
        setAttribute (Qt::WA_TransparentForMouseEvents);
        setAttribute (Qt::WA_NoSystemBackground);
    }

    QColor frameColor;
    QColor labelColor;
    QString label;
    QString tags;
    qreal opacity = 1.0;

protected:
    void paintEvent (QPaintEvent*) override {
        const Theme::Metrics& m = Theme::metrics();
        QPainter painter (this);
        painter.setRenderHint (QPainter::Antialiasing, true);
        painter.setOpacity (opacity);

        const qreal w = m.editBorderWidth;
        const QRectF box = QRectF (rect()).adjusted (w / 2.0, m.legendZone + w / 2.0, -w / 2.0, -w / 2.0);

        QRectF labelRect, tagRect;
        const QFont font = Theme::labelSmallFont();
        const QFontMetricsF fm (font);
        QPainterPath hole;
        if (!label.isEmpty()) {
            const qreal textWidth = fm.horizontalAdvance (label);
            labelRect = QRectF (box.right() - 22.0 - textWidth - 12.0, box.top() - fm.height() / 2.0, textWidth + 12.0, fm.height());
            hole.addRect (labelRect);
        }
        if (!tags.isEmpty()) {
            const qreal textWidth = fm.horizontalAdvance (tags);
            tagRect = QRectF (box.left() + 22.0, box.top() - fm.height() / 2.0, textWidth + 12.0, fm.height());
            hole.addRect (tagRect);
        }
        if (!hole.isEmpty()) {
            QPainterPath visible;
            visible.addRect (QRectF (rect()));
            painter.setClipPath (visible.subtracted (hole));
        }

        QPen pen (frameColor, w);
        pen.setJoinStyle (Qt::RoundJoin);
        painter.setPen (pen);
        painter.setBrush (Qt::NoBrush);
        painter.drawRoundedRect (box, 2.0, 2.0);

        painter.setClipping (false);
        painter.setFont (font);
        if (!label.isEmpty()) {
            painter.setPen (labelColor);
            painter.drawText (labelRect, Qt::AlignCenter, label);
        }
        if (!tags.isEmpty()) {
            painter.setPen (Theme::withAlpha (Theme::colors().accent, 0.95));
            painter.drawText (tagRect, Qt::AlignCenter, tags);
        }
    }
};

BlockEditor::BlockEditor (QWidget* parent)
    : QTextEdit (parent), m_caretColor (Theme::colors().accent)
{
    setCursorWidth (0);
    setViewportMargins (0, Theme::metrics().legendZone, 0, 0);

    m_frame = new FrameOverlay (this);
    applyColors();

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

void BlockEditor::applyColors() {
    const Theme::Colors& c = Theme::colors();
    const Theme::Alphas& a = Theme::alphas();
    m_frame->frameColor = m_wizard ? Theme::withAlpha (c.aqua, a.wizardFrame) : Theme::withAlpha (c.text, a.frame);
    m_frame->labelColor = m_wizard ? Theme::withAlpha (c.aqua, a.wizardLegend) : Theme::withAlpha (c.text, a.legend);
    m_frame->update();
    viewport()->update();
}

void BlockEditor::setLabelText (const QString& text) {
    m_frame->label = text;
    m_frame->update();
}

void BlockEditor::setFrameOpacity (qreal opacity) {
    if (qFuzzyCompare (m_frame->opacity + 1.0, opacity + 1.0))
        return;
    m_frame->opacity = opacity;
    m_frame->setVisible (opacity > 0.01);
    m_frame->update();
}

void BlockEditor::setFormatTags (bool bold, bool italic) {
    QString tags;
    if (bold)
        tags = "B";
    if (italic)
        tags += tags.isEmpty() ? "I" : " I";
    if (tags == m_frame->tags)
        return;
    m_frame->tags = tags;
    m_frame->update();
}

QString BlockEditor::formatTags() const {
    return m_frame->tags;
}

void BlockEditor::setWizardMode (bool wizard) {
    m_wizard = wizard;
    m_caretColor = wizard ? Theme::colors().aqua : Theme::colors().accent;
    applyColors();
}

void BlockEditor::resizeEvent (QResizeEvent* event) {
    QTextEdit::resizeEvent (event);
    m_frame->setGeometry (rect());
    m_frame->raise();
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

BlockView::BlockView (Block* block, QWidget* parent)
    : QWidget (parent), m_block (block), m_type (block->getType())
{
    QVBoxLayout* layout = new QVBoxLayout (this);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->setSpacing (0);

    m_editor = new BlockEditor (this);
    m_editor->setAcceptRichText (false);
    m_editor->setLineWrapMode (QTextEdit::WidgetWidth);
    m_editor->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    m_editor->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    layout->addWidget (m_editor);

    connect (m_editor->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
             this, [this] (const QSizeF&) { updateEditorHeight(); });

    syncFromBlock();
    setFocusedLook (false);
}

bool BlockView::isTextual() const {
    return m_type == BlockType::Title || m_type == BlockType::Subtitle || m_type == BlockType::Paragraph;
}

void BlockView::syncFromBlock() {
    const QString model = m_block->getContent();
    const bool rich = InlineMarkup::isRichType (m_type);
    const QString shown = rich ? InlineMarkup::serialize (m_editor->document()) : m_editor->toPlainText();

    if (model != shown) {
        {
            QSignalBlocker blocker (m_editor);
            if (rich)
                InlineMarkup::load (m_editor->document(), model);
            else
                m_editor->setPlainText (model);
            m_editor->moveCursor (QTextCursor::End);
        }
        applyLook();
        updateEditorHeight();
    }

    if (!isTextual())
        rebuildCard();
}

void BlockView::setFocusedLook (bool focused, bool force) {
    if (m_styled && !force && focused == m_focused)
        return;
    m_styled = true;
    m_focused = focused;

    m_editor->setStyleSheet (Theme::viewStyle());
    m_editor->setReadOnly (!focused);
    m_editor->setTextInteractionFlags (focused ? Qt::TextEditorInteraction : Qt::NoTextInteraction);
    m_editor->setFocusPolicy (focused ? Qt::StrongFocus : Qt::NoFocus);
    m_editor->setAttribute (Qt::WA_TransparentForMouseEvents, !focused);
    m_editor->setWizardMode (false);
    m_editor->setLabelText (commandLabelFor (m_type));

    if (focused) {
        if (m_type == BlockType::Image)
            m_editor->setPlaceholderText ("Ruta de la imagen");
        else if (m_type == BlockType::Equation)
            m_editor->setPlaceholderText ("ecuación en LaTeX");
        else
            m_editor->setPlaceholderText (QString());
    } else {
        m_editor->setPlaceholderText (isTextual() ? QString ("(vacío)") : QString());
    }

    if (!isTextual()) {
        m_editor->setVisible (focused);
        if (m_card)
            m_card->setVisible (!focused);
    }
    setFocusProgress (focused ? 1.0 : 0.0);
    updateEditorHeight();
}

void BlockView::setFocusProgress (qreal progress) {
    m_progress = qBound (0.0, progress, 1.0);
    applyLook();
}

void BlockView::applyLook() {
    const Theme::Colors& c = Theme::colors();
    const Theme::Alphas& a = Theme::alphas();
    const double dim = m_type == BlockType::Title ? a.title : m_type == BlockType::Subtitle ? a.subtitle : a.body;
    const double alpha = dim + (1.0 - dim) * m_progress;

    QPalette palette = m_editor->palette();
    palette.setColor (QPalette::Text, Theme::withAlpha (c.text, alpha));
    m_editor->setPalette (palette);

    const QFont resting = Theme::labelFont (m_type);
    const QFont focused = Theme::editorFont (m_type);
    QFont font = m_progress >= 0.5 ? focused : resting;
    font.setPointSizeF (resting.pointSizeF() + (focused.pointSizeF() - resting.pointSizeF()) * m_progress);
    if (m_editor->font() != font)
        m_editor->setFont (font);

    m_editor->setFrameOpacity (m_progress);
}

void BlockView::setEquationNumber (int number) {
    if (number == m_equationNumber)
        return;
    m_equationNumber = number;
    if (m_type == BlockType::Equation)
        rebuildCard();
}

void BlockView::setCardWidths (int imageBox, int wideBox) {
    m_imageBox = imageBox;
    m_wideBox = wideBox;
    applyCardWidth();
}

void BlockView::applyCardWidth() {
    if (!m_card || m_wideBox <= 0)
        return;
    if (auto* img = qobject_cast<ImageLabel*> (m_card))
        img->setBoxWidth (m_imageBox);
    else if (auto* gr = qobject_cast<GraphLabel*> (m_card))
        gr->setBoxWidth (m_wideBox);
    else if (auto* eq = qobject_cast<EquationLabel*> (m_card))
        eq->setBoxWidth (m_wideBox);
    else if (auto* tb = qobject_cast<TableLabel*> (m_card))
        tb->setBoxWidth (m_wideBox);
}

void BlockView::mouseDoubleClickEvent (QMouseEvent* event) {
    QWidget::mouseDoubleClickEvent (event);
    if (!m_focused)
        emit activated();
}

void BlockView::updateEditorHeight() {
    const int docHeight = static_cast<int> (m_editor->document()->size().height());
    const int minHeight = m_focused ? 50 : 30;
    const int height = qMax (minHeight, docHeight + 16 + Theme::metrics().legendZone);
    if (m_editor->minimumHeight() == height && m_editor->maximumHeight() == height)
        return;
    m_editor->setMinimumHeight (height);
    m_editor->setMaximumHeight (height);
    emit contentHeightChanged();
}

QString BlockView::fingerprint() const {
    QString key = QString::number (static_cast<int> (m_type)) + '|' + m_block->getContent();
    quint64 h = 0;

    if (const GraphBlock* g = dynamic_cast<const GraphBlock*> (m_block)) {
        key += QString ("|%1|%2|%3|%4").arg (static_cast<int> (g->getGraphType()))
                                       .arg (static_cast<int> (g->getScaleX()))
                                       .arg (static_cast<int> (g->getScaleY()))
                                       .arg (g->getSeriesX().name);
        for (double v : g->getSeriesX().values)
            h = h * 31 + qHash (v);
        for (const DataSeries& series : g->getSeriesYList()) {
            h = h * 31 + qHash (series.name);
            for (double v : series.values)
                h = h * 31 + qHash (v);
        }
    } else if (const TableBlock* t = dynamic_cast<const TableBlock*> (m_block)) {
        key += QString ("|%1|%2|%3").arg (t->hasHeaderRow()).arg (t->hasRowNames()).arg (t->getData().size());
        for (const QStringList& row : t->getData())
            for (const QString& cell : row)
                h = h * 31 + qHash (cell);
    } else if (m_type == BlockType::Equation) {
        key += QString ("|%1").arg (m_equationNumber);
    }
    return key + '|' + QString::number (h);
}

void BlockView::rebuildCard() {
    const QString key = fingerprint();
    if (m_card && key == m_cardKey)
        return;
    m_cardKey = key;

    if (m_card) {
        m_card->hide();
        m_card->deleteLater();
        m_card = nullptr;
    }

    if (m_type == BlockType::Image) {
        m_card = new ImageLabel (m_block->getContent(), this);
    } else if (m_type == BlockType::Graph) {
        const GraphBlock* graph = dynamic_cast<const GraphBlock*> (m_block);
        if (!graph)
            return;
        m_card = new GraphLabel (std::make_shared<GraphBlock> (*graph), this);
    } else if (m_type == BlockType::Equation) {
        m_card = new EquationLabel (m_block->getContent(), m_equationNumber, this);
    } else if (m_type == BlockType::Table) {
        const TableBlock* table = dynamic_cast<const TableBlock*> (m_block);
        if (!table)
            return;
        m_card = new TableLabel (table->getData(), table->getTitle(), table->hasHeaderRow(), table->hasRowNames(), this);
    }

    if (!m_card)
        return;

    connect (m_card, &ClickableLabel::doubleClicked, this, &BlockView::activated);
    static_cast<QVBoxLayout*> (layout())->addWidget (m_card, 0, Qt::AlignHCenter);
    m_card->setVisible (!m_focused);
    applyCardWidth();
    updateGeometry();
}

EdgeFade::EdgeFade (QWidget* parent, Backdrop::Widget* backdrop)
    : QWidget (parent), m_backdrop (backdrop)
{
    setAttribute (Qt::WA_TransparentForMouseEvents);
    setAttribute (Qt::WA_NoSystemBackground);
}

void EdgeFade::setMargin (int margin) {
    margin = qMax (0, margin);
    if (margin == m_margin)
        return;
    m_margin = margin;
    update();
}

void EdgeFade::rebuild (int band, qreal dpr, const QPoint& origin) {
    const QPixmap& source = m_backdrop->pixmap();

    auto build = [&] (int offsetY, bool top) {
        const QRect logical (origin.x(), origin.y() + offsetY, width(), band);
        QImage image = source.copy (QRect (logical.topLeft() * dpr, logical.size() * dpr))
                           .toImage()
                           .convertToFormat (QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio (dpr);

        QImage mask (image.size(), QImage::Format_ARGB32_Premultiplied);
        mask.setDevicePixelRatio (dpr);
        mask.fill (Qt::transparent);

        const double solid = static_cast<double> (qMin (m_margin, band)) / band;
        QLinearGradient gradient (0, top ? 0 : band, 0, top ? band : 0);
        gradient.setColorAt (0.0, QColor (0, 0, 0, 255));
        gradient.setColorAt (solid, QColor (0, 0, 0, 255));
        gradient.setColorAt (solid + (1.0 - solid) * 0.45, QColor (0, 0, 0, 140));
        gradient.setColorAt (1.0, QColor (0, 0, 0, 0));

        QPainter maskPainter (&mask);
        maskPainter.fillRect (QRect (0, 0, width(), band), gradient);
        maskPainter.end();

        QPainter painter (&image);
        painter.setCompositionMode (QPainter::CompositionMode_DestinationIn);
        painter.drawImage (QPoint (0, 0), mask);
        painter.end();
        return image;
    };

    m_top = build (0, true);
    m_bottom = build (height() - band, false);

    m_builtSize = size();
    m_builtOrigin = origin;
    m_builtDpr = dpr;
    m_builtBand = band;
    m_builtGeneration = m_backdrop->generation();
}

void EdgeFade::paintEvent (QPaintEvent*) {
    const int band = qMin (height() / 2, m_margin + Theme::metrics().fadeHeight);
    if (band <= 0 || !m_backdrop)
        return;

    const qreal dpr = devicePixelRatioF();
    const QPoint origin = mapTo (m_backdrop, QPoint (0, 0));
    m_backdrop->pixmap();
    if (m_builtSize != size() || m_builtOrigin != origin || m_builtBand != band || !qFuzzyCompare (m_builtDpr, dpr)
        || m_builtGeneration != m_backdrop->generation())
        rebuild (band, dpr, origin);

    QPainter painter (this);
    painter.drawImage (QPoint (0, 0), m_top);
    painter.drawImage (QPoint (0, height() - band), m_bottom);
}

Toast::Toast (QWidget* parent)
    : QWidget (parent)
{
    setAttribute (Qt::WA_TransparentForMouseEvents);
    hide();

    m_animation = new QVariantAnimation (this);
    connect (m_animation, &QVariantAnimation::valueChanged, this, [this] (const QVariant& value) {
        m_opacity = value.toReal();
        update();
    });
    connect (m_animation, &QVariantAnimation::finished, this, [this]() {
        hide();
    });
}

void Toast::setArea (const QRect& area, int bottomMargin) {
    m_area = area;
    m_bottomMargin = bottomMargin;
    if (isVisible())
        place();
}

void Toast::place() {
    const QFontMetrics fm (Theme::labelSmallFont());
    const int maxWidth = qMax (120, static_cast<int> (m_area.width() * 0.7));
    m_shown = fm.elidedText (m_text, Qt::ElideMiddle, maxWidth - 36);
    const QSize size (fm.horizontalAdvance (m_shown) + 36, fm.height() + 16);
    const int x = m_area.center().x() - size.width() / 2;
    const int y = m_area.bottom() - size.height() - 22 - m_bottomMargin / 2;
    setGeometry (QRect (QPoint (x, y), size));
}

void Toast::notify (const QString& text, Kind kind, int holdMs) {
    m_text = text;
    m_kind = kind;
    place();

    const qreal start = isVisible() ? m_opacity : 0.0;
    raise();
    show();

    const int fadeIn = 160;
    const int fadeOut = 420;
    const int total = fadeIn + holdMs + fadeOut;
    m_animation->stop();
    m_animation->setKeyValues (QVariantAnimation::KeyValues());
    m_animation->setDuration (total);
    m_animation->setKeyValueAt (0.0, start);
    m_animation->setKeyValueAt (static_cast<qreal> (fadeIn) / total, 1.0);
    m_animation->setKeyValueAt (static_cast<qreal> (fadeIn + holdMs) / total, 1.0);
    m_animation->setKeyValueAt (1.0, 0.0);
    m_animation->start();
}

void Toast::paintEvent (QPaintEvent*) {
    const Theme::Colors& c = Theme::colors();
    QPainter painter (this);
    painter.setRenderHint (QPainter::Antialiasing, true);
    painter.setOpacity (m_opacity);

    const bool error = m_kind == Kind::Error;
    QColor fill = c.surface;
    fill.setAlphaF (0.94);
    painter.setPen (QPen (error ? Theme::withAlpha (c.accent, 0.85) : Theme::withAlpha (c.text, 0.22), 1.0));
    painter.setBrush (fill);
    painter.drawRoundedRect (QRectF (rect()).adjusted (0.5, 0.5, -0.5, -0.5), 9.0, 9.0);

    painter.setFont (Theme::labelSmallFont());
    painter.setPen (error ? c.accent : Theme::withAlpha (c.text, 0.92));
    painter.drawText (rect(), Qt::AlignCenter, m_shown);
}
