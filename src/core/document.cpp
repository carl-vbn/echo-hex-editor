#include "document.h"

Document::Document(QObject *parent) : QObject(parent) {}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

void Document::loadData(const QByteArray &data)
{
    m_buffer = data;
    m_ops.clear();
    m_opIdx = 0;
    markClean();
    emit sizeChanged(m_buffer.size());
    emit dataChanged(0, m_buffer.size());
    emit undoAvailabilityChanged(false, false);
}

// ---------------------------------------------------------------------------
// Read access
// ---------------------------------------------------------------------------

qint64 Document::size() const
{
    return m_buffer.size();
}

uint8_t Document::byteAt(qint64 offset) const
{
    if (offset < 0 || offset >= m_buffer.size()) return 0;
    return static_cast<uint8_t>(m_buffer.at(static_cast<int>(offset)));
}

QByteArray Document::read(qint64 offset, qint64 length) const
{
    if (offset < 0 || length <= 0 || offset >= m_buffer.size()) return {};
    const qint64 clamped = qMin(length, m_buffer.size() - offset);
    return m_buffer.mid(static_cast<int>(offset), static_cast<int>(clamped));
}

// ---------------------------------------------------------------------------
// Write access
// ---------------------------------------------------------------------------

void Document::overwrite(qint64 offset, uint8_t byte)
{
    overwrite(offset, QByteArray(1, static_cast<char>(byte)));
}

void Document::overwrite(qint64 offset, const QByteArray &data)
{
    if (data.isEmpty() || offset < 0 || offset >= m_buffer.size()) return;
    const qint64 len = qMin(static_cast<qint64>(data.size()), m_buffer.size() - offset);
    Op op;
    op.kind   = Op::Kind::Overwrite;
    op.offset = offset;
    op.before = m_buffer.mid(static_cast<int>(offset), static_cast<int>(len));
    op.after  = data.left(static_cast<int>(len));
    pushOp(std::move(op));
}

void Document::insert(qint64 offset, const QByteArray &data)
{
    if (data.isEmpty()) return;
    const qint64 clampedOff = qBound(0LL, offset, m_buffer.size());
    Op op;
    op.kind   = Op::Kind::Insert;
    op.offset = clampedOff;
    op.before = {};
    op.after  = data;
    pushOp(std::move(op));
}

void Document::remove(qint64 offset, qint64 length)
{
    if (length <= 0 || offset < 0 || offset >= m_buffer.size()) return;
    const qint64 len = qMin(length, m_buffer.size() - offset);
    Op op;
    op.kind   = Op::Kind::Remove;
    op.offset = offset;
    op.before = m_buffer.mid(static_cast<int>(offset), static_cast<int>(len));
    op.after  = {};
    pushOp(std::move(op));
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

bool Document::canUndo() const { return m_opIdx > 0; }
bool Document::canRedo() const { return m_opIdx < m_ops.size(); }

void Document::undo()
{
    if (!canUndo()) return;
    --m_opIdx;
    applyBackward(m_ops[m_opIdx]);
    setModified(m_opIdx > 0);
    emitUndo();
}

void Document::redo()
{
    if (!canRedo()) return;
    applyForward(m_ops[m_opIdx]);
    ++m_opIdx;
    setModified(true);
    emitUndo();
}

// ---------------------------------------------------------------------------
// Dirty flag
// ---------------------------------------------------------------------------

bool Document::isModified() const { return m_modified; }

void Document::markClean()
{
    setModified(false);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void Document::pushOp(Op op)
{
    // Truncate any redoable future
    if (m_opIdx < m_ops.size())
        m_ops.resize(m_opIdx);

    applyForward(op);
    m_ops.push_back(std::move(op));
    m_opIdx = m_ops.size();

    setModified(true);
    emitUndo();
}

void Document::applyForward(const Op &op)
{
    const int off = static_cast<int>(op.offset);
    switch (op.kind) {
    case Op::Kind::Overwrite:
        m_buffer.replace(off, op.after.size(), op.after);
        emit dataChanged(op.offset, op.after.size());
        break;
    case Op::Kind::Insert:
        m_buffer.insert(off, op.after);
        emit sizeChanged(m_buffer.size());
        emit dataChanged(op.offset, m_buffer.size() - op.offset);
        break;
    case Op::Kind::Remove:
        m_buffer.remove(off, static_cast<int>(op.before.size()));
        emit sizeChanged(m_buffer.size());
        emit dataChanged(op.offset, qMax(0LL, m_buffer.size() - op.offset));
        break;
    }
}

void Document::applyBackward(const Op &op)
{
    const int off = static_cast<int>(op.offset);
    switch (op.kind) {
    case Op::Kind::Overwrite:
        m_buffer.replace(off, op.before.size(), op.before);
        emit dataChanged(op.offset, op.before.size());
        break;
    case Op::Kind::Insert:
        m_buffer.remove(off, op.after.size());
        emit sizeChanged(m_buffer.size());
        emit dataChanged(op.offset, qMax(0LL, m_buffer.size() - op.offset));
        break;
    case Op::Kind::Remove:
        m_buffer.insert(off, op.before);
        emit sizeChanged(m_buffer.size());
        emit dataChanged(op.offset, m_buffer.size() - op.offset);
        break;
    }
}

void Document::setModified(bool mod)
{
    if (m_modified == mod) return;
    m_modified = mod;
    emit modifiedChanged(mod);
}

void Document::emitUndo()
{
    emit undoAvailabilityChanged(canUndo(), canRedo());
}
