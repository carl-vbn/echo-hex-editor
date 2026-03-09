#pragma once
#include <QAbstractScrollArea>

// The central hex/ASCII view. Renders a byte buffer in three columns:
//   [offset]  [hex bytes, 16 per row, split 8+8]  [ASCII]
//
// All layout metrics are derived from the actual font at construction/resize
// time so the view is correct regardless of DPI or font substitution.

class Document;
class Node;
class NodeModel;

class HexView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    static constexpr int BYTES_PER_ROW = 16;
    static constexpr int GROUP_SIZE    =  8;  // extra gap after this many bytes

    enum class Column { Hex, Ascii };

    explicit HexView(QWidget *parent = nullptr);

    void setDocument(Document *doc);
    void setNodeModel(NodeModel *model);

    // Mode toggles (connected from toolbar)
    void setDirectEdit(bool on);   // true  → typing always edits
    void setInsertMode(bool on);   // true  → insert; false → overwrite
    void setNodeSelectMode(bool on);
    bool isNodeSelectMode() const { return m_nodeSelectMode; }

    qint64 selectionStart() const;
    qint64 selectionEnd()   const;

    void setSelection(qint64 start, qint64 end);

signals:
    void selectionChanged(qint64 start, qint64 end); // inclusive
    void nodeSelected(Node *node);

protected:
    void paintEvent(QPaintEvent *event)          override;
    void resizeEvent(QResizeEvent *event)        override;
    void keyPressEvent(QKeyEvent *event)         override;
    void mousePressEvent(QMouseEvent *event)     override;
    void mouseMoveEvent(QMouseEvent *event)      override;
    void mouseReleaseEvent(QMouseEvent *event)   override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event)        override;
    void focusOutEvent(QFocusEvent *event)       override;

private:
    Document  *m_document  = nullptr;
    NodeModel *m_nodeModel = nullptr;

    // Selection
    // m_selStart is the anchor, m_selEnd is the drag end.
    // Normalise with qMin/qMax for range tests.
    qint64 m_selStart = -1;
    qint64 m_selEnd   = -1;
    bool   m_dragging = false;

    // Cursor / edit state
    qint64 m_cursor     = 0;
    Column m_cursorCol  = Column::Hex;
    bool   m_editActive = false;  // enter/exit via double-click or Enter
    bool   m_directEdit = false;  // if true, always edit (HxD style)
    bool   m_insertMode = false;  // insert vs overwrite
    bool   m_nodeSelectMode = false;

    // Pending nibble when editing hex column (m_hexNibble == 1 → waiting for low)
    int     m_hexNibble  = 0;
    uint8_t m_highNibble = 0;

    // Font-derived metrics (recomputed by computeLayout)
    int m_charW = 8;   // width  of one character cell
    int m_charH = 14;  // height of one character cell (fm.height())
    int m_rowH  = 18;  // row height = charH + vertical padding

    // Column x-positions (content coordinates, before horizontal scroll)
    int m_addrX  = 0;
    int m_hexX   = 0;
    int m_asciiX = 0;
    int m_sep1X  = 0;
    int m_sep2X  = 0;

    void   computeLayout();
    void   updateScrollRange();

    int    hexByteX(int byteInRow)   const;
    int    asciiByteX(int byteInRow) const;
    int    rowToY(qint64 row)        const;

    // Returns byte index under pos, sets *col if non-null. Returns -1 if none.
    qint64 hitTest(const QPoint &viewportPos, Column *col = nullptr) const;

    bool   inSelection(qint64 idx) const;

    // Cursor / edit helpers
    void moveCursor(qint64 idx, Column col, bool extendSelection = false);
    void enterEditMode();
    void exitEditMode();
    bool isEditing() const { return m_editActive || m_directEdit; }

    void handleHexKey(int hexDigit);   // hexDigit in [0, 15]
    void handleAsciiKey(uint8_t ch);
    void commitHexByte(uint8_t value);

    void ensureCursorVisible();
};
