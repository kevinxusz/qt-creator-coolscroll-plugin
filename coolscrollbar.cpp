#include "coolscrollbar.h"
#include <QtGui/QPlainTextEdit>
#include <QtGui/QTextBlock>
#include <QtGui/QTextLayout>
#include <QtGui/QTextDocumentFragment>

#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QDebug>

#include <texteditor/basetexteditor.h>
#include <texteditor/texteditorsettings.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditorconstants.h>

#include "coolscrollbarsettings.h"

CoolScrollBar::CoolScrollBar(TextEditor::BaseTextEditorWidget* edit,
                             QSharedPointer<CoolScrollbarSettings>& settings) :
    m_parentEdit(edit),
    m_settings(settings),
    m_yAdditionalScale(1.0),
    m_highlightNextSelection(false),
    m_leftButtonPressed(false)
{
    m_parentEdit->viewport()->installEventFilter(this);

    m_internalDocument = originalDocument().clone();
    applySettingsToDocument(internalDocument());
    connect(m_parentEdit, SIGNAL(textChanged()), SLOT(onDocumentContentChanged()));
    connect(m_parentEdit, SIGNAL(selectionChanged()), SLOT(onDocumentSelectionChanged()));

    updatePicture();
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.drawPixmap(0, 0, width(), height(), m_previewPic);
    drawViewportRect(p);
    p.end();
}
////////////////////////////////////////////////////////////////////////////
int CoolScrollBar::unfoldedLinesCount() const
{
    Q_ASSERT(m_parentEdit);
    int res = 0;
    QTextBlock b = originalDocument().firstBlock();
    while(b != originalDocument().lastBlock())
    {
        if(b.isVisible())
        {
            res += b.lineCount();
        }
        b = b.next();
    }
    return res;
}
////////////////////////////////////////////////////////////////////////////
int CoolScrollBar::linesInViewportCount() const
{
    return (2 * m_parentEdit->document()->lineCount() - unfoldedLinesCount() - maximum());
}
////////////////////////////////////////////////////////////////////////////
QSize CoolScrollBar::sizeHint() const
{
    return QSize(settings().m_scrollBarWidth, 0);
}
////////////////////////////////////////////////////////////////////////////
QSize CoolScrollBar::minimumSizeHint() const
{
    return QSize(settings().m_scrollBarWidth, 0);
}
////////////////////////////////////////////////////////////////////////////
const QTextDocument & CoolScrollBar::originalDocument() const
{
    return *m_parentEdit->document();
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::onDocumentContentChanged()
{
    internalDocument().setPlainText(originalDocument().toPlainText());
    QTextBlock origBlock = originalDocument().firstBlock();
    QTextBlock internBlock = internalDocument().firstBlock();

    while(origBlock.isValid() && internBlock.isValid())
    {
        internBlock.layout()->setAdditionalFormats(origBlock.layout()->additionalFormats());
        origBlock = origBlock.next();
        internBlock = internBlock.next();
    }
    updatePicture();
    update();
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::onDocumentSelectionChanged()
{
    if(m_highlightNextSelection)
    {
        // clear previous highlight
        clearHighlight();
        m_stringToHighlight = m_parentEdit->textCursor().selection().toPlainText();
        // highlight new selection
        highlightSelectedWord();

        updatePicture();
        update();
    }
}
////////////////////////////////////////////////////////////////////////////
bool CoolScrollBar::eventFilter(QObject *obj, QEvent *e)
{
    if(obj == m_parentEdit->viewport())
    {
        m_highlightNextSelection = (e->type() == QEvent::MouseButtonDblClick);
    }
    return false;
}
////////////////////////////////////////////////////////////////////////////
qreal CoolScrollBar::getXScale() const
{
    return settings().m_xDefaultScale;
}
////////////////////////////////////////////////////////////////////////////
qreal CoolScrollBar::getYScale() const
{
    return settings().m_yDefaultScale * m_yAdditionalScale;
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::drawViewportRect(QPainter &p)
{
    qreal lineHeight = calculateLineHeight();
    lineHeight *= getYScale();
    QPointF rectPos(0, value() * lineHeight);
    QRectF rect(rectPos, QSizeF(settings().m_scrollBarWidth,
                                linesInViewportCount() * lineHeight));

    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(settings().m_viewportColor));
    p.drawRect(rect);
}
////////////////////////////////////////////////////////////////////////////
qreal CoolScrollBar::calculateLineHeight() const
{
    QFontMetrics fm(settings().m_font);
    return qreal(fm.height());
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::drawPreview(QPainter &p)
{
    //internalDocument().drawContents(&p);
    QTextBlock block = internalDocument().begin();
    QPointF pos(0.0f, 0.0f);
    while(block.isValid())
    {
        block.layout()->draw(&p, pos, block.layout()->additionalFormats().toVector());
        pos += QPointF(0, block.lineCount() * calculateLineHeight());
        block = block.next();
    }
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::applySettingsToDocument(QTextDocument &doc) const
{
    doc.setDefaultFont(settings().m_font);
    doc.setDefaultTextOption(settings().m_textOption);
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::highlightEntryInDocument(const QString& str, const QTextCharFormat& format)
{
    if(str.isEmpty())
    {
        return;
    }
    QTextCursor cur_cursor(&internalDocument());
    while(true)
    {
        cur_cursor = internalDocument().find(str, cur_cursor);
        if(!cur_cursor.isNull())
        {
            cur_cursor.mergeCharFormat(format);
        }
        else
        {
            break;
        }
    }
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        setValue(posToValue(event->posF().y()));
        m_leftButtonPressed = true;
    }
    else if(event->button() == Qt::RightButton)
    {
        clearHighlight();
        updatePicture();
        update();
    }
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::contextMenuEvent(QContextMenuEvent *event)
{
    if(!settings().m_disableContextMenu)
    {
        QScrollBar::contextMenuEvent(event);
    }
}

void CoolScrollBar::mouseMoveEvent(QMouseEvent *event)
{
    if(m_leftButtonPressed)
    {
        setValue(posToValue(event->posF().y()));
    }
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::updatePicture()
{
    updateScaleFactors();
    m_previewPic = QPixmap(width() / getXScale(), height() / getYScale());
    m_previewPic.fill(Qt::white);
    QPainter pic(&m_previewPic);
    drawPreview(pic);
    pic.end();

    // scale pixmap with bilinear filtering
    m_previewPic = m_previewPic.scaled(width(), height(), Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::updateScaleFactors()
{
    int lineHeight = calculateLineHeight();

    qreal documentHeight = qreal(lineHeight * m_internalDocument->lineCount());
    documentHeight *= settings().m_yDefaultScale;

    if(documentHeight > size().height())
    {
        m_yAdditionalScale = size().height() / documentHeight;
    }
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::resizeEvent(QResizeEvent *)
{
    updatePicture();
}
////////////////////////////////////////////////////////////////////////////
int CoolScrollBar::posToValue(qreal pos) const
{
    qreal documentHeight = internalDocument().lineCount() * calculateLineHeight() * getYScale();
    int value = int(pos * (maximum() + linesInViewportCount()) / documentHeight);

    // set center of viewport to position of click
    value -= linesInViewportCount() / 2;
    if(value > maximum())
    {
        value = maximum();
    }
    else if (value < minimum())
    {
        value = minimum();
    }
    return value;
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_leftButtonPressed = false;
    }
}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::highlightSelectedWord()
{
    QTextCharFormat format;
    format.setBackground(settings().m_selectionHighlightColor);
    QFont font = settings().m_font;
    font.setPointSizeF(font.pointSizeF() * 2.0);
    format.setFont(font);
    highlightEntryInDocument(m_stringToHighlight, format);

}
////////////////////////////////////////////////////////////////////////////
void CoolScrollBar::clearHighlight()
{
    QTextCharFormat format;
    format.setBackground(Qt::white);
    format.setFont(settings().m_font);
    highlightEntryInDocument(m_stringToHighlight, format);
}
