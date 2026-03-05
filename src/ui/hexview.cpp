#include "hexview.h"
#include "core/document.h"
#include "core/node.h"
#include "core/nodemodel.h"
#include "theme/theme.h"

#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QKeyEvent>

// View-local colour constants
namespace {

const QColor COL_BG          { Theme::Color::BG_PANEL };
const QColor COL_ADDR        { "#4A4A4A" };  // offset column
const QColor COL_BYTE        { "#989898" };  // normal non-zero byte
const QColor COL_ZERO        { "#272727" };  // zero byte (barely visible)
const QColor COL_NONPRINT    { "#4A4A4A" };  // non-printable ASCII glyph (·)
const QColor COL_SEP         { "#161616" };  // column separator lines
const QColor COL_SEL_BG      { "#152638" };  // selection fill
const QColor COL_SEL_TEXT    { "#D8D8D8" };  // text on selection
const QColor COL_CURSOR_BG        { "#1C1C1C" };  // cursor fill (navigation)
const QColor COL_CURSOR_BORD      { "#505050" };  // cursor border (navigation)
const QColor COL_CURSOR_EDIT_BG   { "#223040" };  // cursor fill (edit mode)
const QColor COL_CURSOR_EDIT_BORD { "#5090C0" };  // cursor border (edit mode)

// Pixel constants
constexpr int LEFT_MARGIN   = 14;
constexpr int ADDR_HEX_GAP  = 20;
constexpr int HEX_ASCII_GAP = 16;
constexpr int ROW_V_PAD     =  5;

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

HexView::HexView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFrameStyle(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    computeLayout();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void HexView::setDocument(Document *doc)
{
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }
    m_document  = doc;
    m_selStart  = -1;
    m_selEnd    = -1;
    m_cursor    = 0;
    m_editActive = false;
    m_hexNibble = 0;

    if (m_document) {
        connect(m_document, &Document::dataChanged, this,
                [this](qint64, qint64) { viewport()->update(); });
        connect(m_document, &Document::sizeChanged, this,
                [this](qint64) { updateScrollRange(); viewport()->update(); });
    }

    updateScrollRange();
    viewport()->update();
}

void HexView::setDirectEdit(bool on)
{
    if (m_directEdit == on) return;
    m_directEdit = on;
    if (!on) exitEditMode();
    viewport()->update();
}

void HexView::setInsertMode(bool on)
{
    m_insertMode = on;
}

void HexView::setNodeSelectMode(bool on)
{
    m_nodeSelectMode = on;
    if (on) exitEditMode();
    viewport()->update();
}

void HexView::setNodeModel(NodeModel *model)
{
    if (m_nodeModel)
        disconnect(m_nodeModel, nullptr, this, nullptr);
    m_nodeModel = model;
    if (m_nodeModel) {
        auto refresh = [this] { viewport()->update(); };
        connect(m_nodeModel, &NodeModel::nodeCreated, this, refresh);
        connect(m_nodeModel, &NodeModel::nodeRemoved, this, refresh);
        connect(m_nodeModel, &NodeModel::nodeChanged, this, refresh);
        connect(m_nodeModel, &NodeModel::modelReset,  this, refresh);
    }
    viewport()->update();
}

void HexView::setSelection(qint64 start, qint64 end)
{
    if (!m_document) return;
    m_selStart = start;
    m_selEnd   = end;
    m_cursor   = end;
    ensureCursorVisible();
    viewport()->update();
    emit selectionChanged(selectionStart(), selectionEnd());
}

qint64 HexView::selectionStart() const
{
    if (m_selStart < 0 || m_selEnd < 0) return -1;
    return qMin(m_selStart, m_selEnd);
}

qint64 HexView::selectionEnd() const
{
    if (m_selStart < 0 || m_selEnd < 0) return -1;
    return qMax(m_selStart, m_selEnd);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void HexView::computeLayout()
{
    const QFontMetrics fm(viewport()->font());
    m_charW = fm.horizontalAdvance(QLatin1Char('A'));
    m_charH = fm.height();
    m_rowH  = m_charH + ROW_V_PAD;

    m_addrX = LEFT_MARGIN;
    m_hexX  = m_addrX + 8 * m_charW + ADDR_HEX_GAP;

    const int hexW = BYTES_PER_ROW * 3 * m_charW;

    m_asciiX = m_hexX + hexW + HEX_ASCII_GAP;

    m_sep1X  = m_hexX  - ADDR_HEX_GAP  / 2;
    m_sep2X  = m_hexX  + hexW + HEX_ASCII_GAP / 2;

    const int totalW = m_asciiX + BYTES_PER_ROW * m_charW + LEFT_MARGIN;
    const int vpW    = viewport()->width();
    horizontalScrollBar()->setRange(0, qMax(0, totalW - vpW));
    horizontalScrollBar()->setPageStep(vpW);
}

void HexView::updateScrollRange()
{
    if (m_rowH == 0 || !m_document) {
        verticalScrollBar()->setRange(0, 0);
        return;
    }
    const qint64 rows     = (m_document->size() + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
    const int    contentH = static_cast<int>(rows * m_rowH);
    const int    vpH      = viewport()->height();
    verticalScrollBar()->setRange(0, qMax(0, contentH - vpH));
    verticalScrollBar()->setPageStep(vpH);
    verticalScrollBar()->setSingleStep(m_rowH);
}

// ---------------------------------------------------------------------------
// Column coordinate helpers
// ---------------------------------------------------------------------------

int HexView::hexByteX(int i) const
{
    return m_hexX + (i * 3 + (i >= GROUP_SIZE ? 1 : 0)) * m_charW
           - horizontalScrollBar()->value();
}

int HexView::asciiByteX(int i) const
{
    return m_asciiX + i * m_charW - horizontalScrollBar()->value();
}

int HexView::rowToY(qint64 row) const
{
    return static_cast<int>(row * m_rowH) - verticalScrollBar()->value();
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------

qint64 HexView::hitTest(const QPoint &pos, Column *col) const
{
    if (!m_document || m_document->size() == 0) return -1;

    const int hscroll = horizontalScrollBar()->value();
    const int vscroll = verticalScrollBar()->value();

    const qint64 row      = (static_cast<qint64>(pos.y()) + vscroll) / m_rowH;
    const qint64 rowStart = row * BYTES_PER_ROW;
    if (rowStart >= m_document->size()) return -1;

    const int rowBytes = static_cast<int>(
        qMin(static_cast<qint64>(BYTES_PER_ROW), m_document->size() - rowStart));

    const int adjX = pos.x() + hscroll;

    // Hex area
    const int relHex        = adjX - m_hexX;
    const int group1StartPx = (GROUP_SIZE * 3 + 1) * m_charW;
    if (relHex >= 0 && adjX < m_asciiX) {
        int b;
        if (relHex < group1StartPx)
            b = qBound(0, relHex / (3 * m_charW), GROUP_SIZE - 1);
        else
            b = qBound(GROUP_SIZE,
                       GROUP_SIZE + (relHex - group1StartPx) / (3 * m_charW),
                       BYTES_PER_ROW - 1);
        b = qMin(b, rowBytes - 1);
        if (col) *col = Column::Hex;
        return qMin(rowStart + b, m_document->size() - 1LL);
    }

    // ASCII area
    const int relAscii = adjX - m_asciiX;
    if (relAscii >= 0 && relAscii < rowBytes * m_charW) {
        const int b = qBound(0, relAscii / m_charW, rowBytes - 1);
        if (col) *col = Column::Ascii;
        return qMin(rowStart + b, m_document->size() - 1LL);
    }

    return -1;
}

bool HexView::inSelection(qint64 idx) const
{
    if (m_selStart < 0 || m_selEnd < 0) return false;
    return idx >= qMin(m_selStart, m_selEnd) &&
           idx <= qMax(m_selStart, m_selEnd);
}

// ---------------------------------------------------------------------------
// Cursor / edit helpers
// ---------------------------------------------------------------------------

void HexView::moveCursor(qint64 idx, Column col, bool extendSelection)
{
    if (!m_document || idx < 0 || idx >= m_document->size()) return;

    m_cursor    = idx;
    m_cursorCol = col;
    m_hexNibble = 0;  // reset pending nibble on cursor move

    if (extendSelection) {
        m_selEnd = idx;
    } else {
        m_selStart = idx;
        m_selEnd   = idx;
    }

    ensureCursorVisible();
    viewport()->update();
    emit selectionChanged(selectionStart(), selectionEnd());
}

void HexView::enterEditMode()
{
    m_editActive = true;
    m_hexNibble  = 0;
    // Collapse selection to the cursor so only the cursor is highlighted
    m_selStart = m_cursor;
    m_selEnd   = m_cursor;
    viewport()->update();
}

void HexView::exitEditMode()
{
    m_editActive = false;
    m_hexNibble  = 0;
    viewport()->update();
}

void HexView::handleHexKey(int hexDigit)
{
    if (!m_document || m_cursor < 0 || m_cursor >= m_document->size()) return;

    if (m_hexNibble == 0) {
        // Received high nibble — wait for low nibble
        m_highNibble = static_cast<uint8_t>(hexDigit);
        m_hexNibble  = 1;
        viewport()->update();
    } else {
        // Received low nibble — commit byte
        const uint8_t value = (m_highNibble << 4) | static_cast<uint8_t>(hexDigit);
        commitHexByte(value);
        m_hexNibble = 0;
    }
}

void HexView::commitHexByte(uint8_t value)
{
    if (!m_document) return;
    if (m_insertMode) {
        m_document->insert(m_cursor, QByteArray(1, static_cast<char>(value)));
        if (m_cursor + 1 < m_document->size())
            moveCursor(m_cursor + 1, m_cursorCol);
    } else {
        m_document->overwrite(m_cursor, value);
        const qint64 next = m_cursor + 1;
        if (next < m_document->size())
            moveCursor(next, m_cursorCol);
    }
}

void HexView::handleAsciiKey(uint8_t ch)
{
    if (!m_document) return;
    if (m_insertMode) {
        m_document->insert(m_cursor, QByteArray(1, static_cast<char>(ch)));
        if (m_cursor + 1 < m_document->size())
            moveCursor(m_cursor + 1, m_cursorCol);
    } else {
        m_document->overwrite(m_cursor, ch);
        const qint64 next = m_cursor + 1;
        if (next < m_document->size())
            moveCursor(next, m_cursorCol);
    }
}

void HexView::ensureCursorVisible()
{
    if (!m_document || m_cursor < 0) return;
    const qint64 row = m_cursor / BYTES_PER_ROW;
    const int    y   = rowToY(row);
    const int    vpH = viewport()->height();

    if (y < 0)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + y);
    else if (y + m_rowH > vpH)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + (y + m_rowH - vpH));
}

// ---------------------------------------------------------------------------
// Key events
// ---------------------------------------------------------------------------

void HexView::keyPressEvent(QKeyEvent *e)
{
    if (!m_document || m_document->size() == 0) {
        QAbstractScrollArea::keyPressEvent(e);
        return;
    }

    const qint64 docSize = m_document->size();
    const qint64 cur     = m_cursor;

    // -- Navigation (available regardless of edit mode) ---------------------
    switch (e->key()) {

    case Qt::Key_Left:
        if (cur > 0) moveCursor(cur - 1, m_cursorCol,
                                e->modifiers() & Qt::ShiftModifier);
        return;

    case Qt::Key_Right:
        if (cur + 1 < docSize) moveCursor(cur + 1, m_cursorCol,
                                           e->modifiers() & Qt::ShiftModifier);
        return;

    case Qt::Key_Up:
        if (cur >= BYTES_PER_ROW) moveCursor(cur - BYTES_PER_ROW, m_cursorCol,
                                              e->modifiers() & Qt::ShiftModifier);
        return;

    case Qt::Key_Down:
        if (cur + BYTES_PER_ROW < docSize) moveCursor(cur + BYTES_PER_ROW, m_cursorCol,
                                                       e->modifiers() & Qt::ShiftModifier);
        return;

    case Qt::Key_Home:
        moveCursor((cur / BYTES_PER_ROW) * BYTES_PER_ROW, m_cursorCol,
                   e->modifiers() & Qt::ShiftModifier);
        return;

    case Qt::Key_End: {
        const qint64 rowEnd = qMin((cur / BYTES_PER_ROW + 1) * BYTES_PER_ROW - 1,
                                   docSize - 1);
        moveCursor(rowEnd, m_cursorCol, e->modifiers() & Qt::ShiftModifier);
        return;
    }

    case Qt::Key_PageUp: {
        const int rows = viewport()->height() / m_rowH;
        moveCursor(qMax(0LL, cur - rows * BYTES_PER_ROW), m_cursorCol,
                   e->modifiers() & Qt::ShiftModifier);
        return;
    }

    case Qt::Key_PageDown: {
        const int rows = viewport()->height() / m_rowH;
        moveCursor(qMin(docSize - 1, cur + rows * BYTES_PER_ROW), m_cursorCol,
                   e->modifiers() & Qt::ShiftModifier);
        return;
    }

    case Qt::Key_Tab:
        m_cursorCol = (m_cursorCol == Column::Hex) ? Column::Ascii : Column::Hex;
        m_hexNibble = 0;
        viewport()->update();
        return;

    case Qt::Key_Escape:
        exitEditMode();
        return;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!m_directEdit)
            isEditing() ? exitEditMode() : enterEditMode();
        return;

    default:
        break;
    }

    // -- Editing keys --------------------------------------------------------
    if (m_nodeSelectMode || !isEditing()) {
        QAbstractScrollArea::keyPressEvent(e);
        return;
    }

    // Backspace
    if (e->key() == Qt::Key_Backspace) {
        if (m_cursorCol == Column::Hex && m_hexNibble == 1) {
            // Cancel pending high nibble
            m_hexNibble = 0;
            viewport()->update();
        } else if (m_insertMode && cur > 0) {
            m_document->remove(cur - 1, 1);
            moveCursor(qMax(0LL, cur - 1), m_cursorCol);
        }
        // In overwrite mode backspace just moves cursor back (no delete)
        else if (!m_insertMode && cur > 0) {
            moveCursor(cur - 1, m_cursorCol);
        }
        return;
    }

    // Delete
    if (e->key() == Qt::Key_Delete) {
        if (m_insertMode && cur < docSize)
            m_document->remove(cur, 1);
        return;
    }

    // Hex column: accept 0-9, a-f
    if (m_cursorCol == Column::Hex) {
        const QString text = e->text().toLower();
        if (!text.isEmpty()) {
            const QChar c = text.at(0);
            int digit = -1;
            if (c >= '0' && c <= '9') digit = c.digitValue();
            else if (c >= 'a' && c <= 'f') digit = c.unicode() - 'a' + 10;
            if (digit >= 0) {
                handleHexKey(digit);
                return;
            }
        }
    }

    // ASCII column: accept printable characters
    if (m_cursorCol == Column::Ascii) {
        const QString text = e->text();
        if (!text.isEmpty()) {
            const ushort u = text.at(0).unicode();
            if (u >= 0x20 && u < 0x7F) {
                handleAsciiKey(static_cast<uint8_t>(u));
                return;
            }
        }
    }

    QAbstractScrollArea::keyPressEvent(e);
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void HexView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    Column col = Column::Hex;
    const qint64 idx = hitTest(e->pos(), &col);
    if (idx < 0) return;

    setFocus();

    if (m_nodeSelectMode) {
        if (!m_nodeModel) return;
        Node *node = m_nodeModel->deepestContaining(idx, 1);
        if (!node) return;
        setSelection(node->absoluteStart(),
                     node->absoluteStart() + node->length() - 1);
        emit nodeSelected(node);
        return;
    }

    m_dragging = true;

    // Clicking a different byte exits explicit edit mode
    if (m_editActive && idx != m_cursor)
        exitEditMode();

    if (m_hexNibble == 1) m_hexNibble = 0;  // discard pending nibble

    moveCursor(idx, col, e->modifiers() & Qt::ShiftModifier);
}

void HexView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_nodeSelectMode) return;
    if (!m_dragging || !(e->buttons() & Qt::LeftButton)) return;
    Column col = m_cursorCol;
    const qint64 idx = hitTest(e->pos(), &col);
    if (idx < 0 || idx == m_selEnd) return;

    m_cursor = idx;
    m_selEnd = idx;
    viewport()->update();
    emit selectionChanged(selectionStart(), selectionEnd());
}

void HexView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        m_dragging = false;
}

void HexView::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (m_nodeSelectMode) return;
    if (e->button() != Qt::LeftButton) return;
    Column col = Column::Hex;
    const qint64 idx = hitTest(e->pos(), &col);
    if (idx < 0) return;

    moveCursor(idx, col);
    if (!m_directEdit)
        enterEditMode();
}

// ---------------------------------------------------------------------------
// Focus events
// ---------------------------------------------------------------------------

void HexView::focusInEvent(QFocusEvent *e)
{
    QAbstractScrollArea::focusInEvent(e);
    viewport()->update();
}

void HexView::focusOutEvent(QFocusEvent *e)
{
    exitEditMode();
    QAbstractScrollArea::focusOutEvent(e);
    viewport()->update();
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------

void HexView::resizeEvent(QResizeEvent *e)
{
    QAbstractScrollArea::resizeEvent(e);
    computeLayout();
    updateScrollRange();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void HexView::paintEvent(QPaintEvent *)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, false);

    const QFontMetrics fm(viewport()->font());
    const QRect vp = viewport()->rect();

    p.fillRect(vp, COL_BG);
    if (!m_document || m_document->size() == 0) return;

    const bool focused = hasFocus();
    const qint64 docSize = m_document->size();

    const int    vscroll = verticalScrollBar()->value();
    const qint64 rows    = (docSize + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
    const qint64 first   = vscroll / m_rowH;
    const qint64 last    = qMin(rows, static_cast<qint64>((vscroll + vp.height()) / m_rowH + 1));

    // Column separator lines
    const int sep1 = m_sep1X - horizontalScrollBar()->value();
    const int sep2 = m_sep2X - horizontalScrollBar()->value();
    p.setPen(QPen(COL_SEP, 1));
    p.drawLine(sep1, 0, sep1, vp.height());
    p.drawLine(sep2, 0, sep2, vp.height());

    for (qint64 row = first; row < last; ++row) {
        const int y        = rowToY(row);
        const int baseline = y + ROW_V_PAD / 2 + fm.ascent();

        const qint64 rowStart = row * BYTES_PER_ROW;
        const int rowBytes = static_cast<int>(
            qMin(static_cast<qint64>(BYTES_PER_ROW), docSize - rowStart));

        // -- Address column --------------------------------------------------
        const int addrX = m_addrX - horizontalScrollBar()->value();
        p.setPen(COL_ADDR);
        p.drawText(addrX, baseline,
                   QString("%1").arg(rowStart, 8, 16, QLatin1Char('0')).toUpper());

        // Only suppress selection and use edit-mode cursor colours when
        // explicitly in edit mode. In direct-edit mode selection is normal.
        const bool editing = m_editActive;

        // -- Hex bytes -------------------------------------------------------
        for (int i = 0; i < rowBytes; ++i) {
            const qint64  idx = rowStart + i;
            const uint8_t b   = m_document->byteAt(idx);
            const int     bx  = hexByteX(i);
            const bool    sel = !editing && inSelection(idx);
            const bool    cur = (focused && idx == m_cursor);

            // Background — selection suppressed in edit mode
            if (sel)
                p.fillRect(bx - 1, y, m_charW * 2 + 2, m_rowH, COL_SEL_BG);
            else if (cur)
                p.fillRect(bx - 1, y, m_charW * 2 + 2, m_rowH,
                           editing ? COL_CURSOR_EDIT_BG : COL_CURSOR_BG);

            // Cursor border — brighter when any form of editing is active
            if (cur && m_cursorCol == Column::Hex) {
                p.setPen(QPen(isEditing() ? COL_CURSOR_EDIT_BORD : COL_CURSOR_BORD, 1));
                p.setBrush(Qt::NoBrush);
                p.drawRect(bx - 1, y, m_charW * 2 + 1, m_rowH - 1);
            }

            // Text — when waiting for low nibble, show high nibble + '_'
            if (cur && isEditing() && m_cursorCol == Column::Hex && m_hexNibble == 1) {
                p.setPen(COL_SEL_TEXT);
                p.drawText(bx, baseline,
                           QString::number(m_highNibble, 16).toUpper() + "_");
            } else {
                QColor textCol;
                if (m_nodeModel) {
                    const QColor nc = m_nodeModel->colorAt(idx);
                    textCol = nc.isValid() ? nc : (sel ? COL_SEL_TEXT : (b == 0 ? COL_ZERO : COL_BYTE));
                } else {
                    textCol = sel ? COL_SEL_TEXT : (b == 0 ? COL_ZERO : COL_BYTE);
                }
                p.setPen(textCol);
                p.drawText(bx, baseline,
                           QString("%1").arg(b, 2, 16, QLatin1Char('0')).toUpper());
            }
        }

        // -- ASCII column ----------------------------------------------------
        for (int i = 0; i < rowBytes; ++i) {
            const qint64  idx   = rowStart + i;
            const uint8_t b     = m_document->byteAt(idx);
            const int     ax    = asciiByteX(i);
            const bool    sel   = !editing && inSelection(idx);
            const bool    cur   = (focused && idx == m_cursor);
            const bool    print = (b >= 0x20 && b < 0x7F);

            if (sel)
                p.fillRect(ax, y, m_charW, m_rowH, COL_SEL_BG);
            else if (cur)
                p.fillRect(ax, y, m_charW, m_rowH,
                           editing ? COL_CURSOR_EDIT_BG : COL_CURSOR_BG);

            if (cur && m_cursorCol == Column::Ascii) {
                p.setPen(QPen(isEditing() ? COL_CURSOR_EDIT_BORD : COL_CURSOR_BORD, 1));
                p.setBrush(Qt::NoBrush);
                p.drawRect(ax, y, m_charW - 1, m_rowH - 1);
            }

            QColor textCol;
            if (m_nodeModel) {
                const QColor nc = m_nodeModel->colorAt(idx);
                textCol = nc.isValid() ? nc : (sel ? COL_SEL_TEXT : (print ? COL_BYTE : COL_NONPRINT));
            } else {
                textCol = sel ? COL_SEL_TEXT : (print ? COL_BYTE : COL_NONPRINT);
            }
            p.setPen(textCol);
            p.drawText(ax, baseline, print ? QString(QChar(b)) : QString("\xC2\xB7"));
        }
    }
}
