#pragma once
#include <QObject>
#include <QByteArray>
#include <QVector>

// Document: the editable byte buffer.
//
// This is the interface described in DATA_MODEL.md.  The current
// implementation is a simple single QByteArray with an in-memory undo stack;
// the block-list storage system can be dropped in later without changing the
// public API.

class Document : public QObject
{
    Q_OBJECT

public:
    explicit Document(QObject *parent = nullptr);

    // Load replaces the whole buffer and clears undo history.
    void loadData(const QByteArray &data);

    // Read access
    qint64     size()                               const;
    uint8_t    byteAt(qint64 offset)                const;
    QByteArray read(qint64 offset, qint64 length)   const;

    // Write access
    // Each call pushes one undo op.
    void overwrite(qint64 offset, uint8_t byte);
    void overwrite(qint64 offset, const QByteArray &data);
    void insert(qint64 offset, const QByteArray &data);
    void remove(qint64 offset, qint64 length);

    // Undo / redo
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    // Dirty flag
    bool isModified() const;
    void markClean();

signals:
    void dataChanged(qint64 offset, qint64 length);  // bytes changed in place
    void sizeChanged(qint64 newSize);                // buffer grew or shrank
    void undoAvailabilityChanged(bool canUndo, bool canRedo);
    void modifiedChanged(bool modified);

private:
    QByteArray m_buffer;
    bool       m_modified = false;

    struct Op {
        enum class Kind { Overwrite, Insert, Remove };
        Kind       kind;
        qint64     offset;
        QByteArray before;  // data that was here before the op (for undo)
        QByteArray after;   // data written by the op (for redo)
    };

    QVector<Op> m_ops;
    int         m_opIdx = 0;  // index of the next op to redo (== applied count)

    void pushOp(Op op);
    void applyForward(const Op &op);
    void applyBackward(const Op &op);
    void setModified(bool mod);
    void emitUndo();
};
