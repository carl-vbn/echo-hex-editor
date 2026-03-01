#pragma once
#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

class NodeModel;

class Node
{
public:
    enum class Type {
        Blob,
        U8, I8, U16, I16, U32, I32, U64, I64,
        F32, F64
    };

    static QString     typeName(Type t);
    static QStringList allTypeNames();
    static Type        typeFromName(const QString &name);
    static int         typeSize(Type t);   // 0 for Blob

    quint64            id()            const { return m_id; }
    QString            name()          const { return m_name; }
    qint64             startOffset()   const { return m_startOffset; }  // relative to parent
    qint64             length()        const { return m_length; }
    Type               type()          const { return m_type; }
    QColor             color()         const { return m_color; }
    bool               isLittleEndian() const { return m_littleEndian; }

    Node              *parent()   const { return m_parent; }
    const QVector<Node*> &children() const { return m_children; }
    bool               isLeaf()   const { return m_children.isEmpty(); }
    bool               isRoot()   const { return m_parent == nullptr; }

    qint64             absoluteStart() const;

private:
    friend class NodeModel;

    Node(quint64 id, Node *parent, qint64 startOffset, qint64 length, const QString &name);
    ~Node();

    quint64       m_id;
    QString       m_name;
    qint64        m_startOffset;
    qint64        m_length;
    Type          m_type         = Type::Blob;
    QColor        m_color;
    bool          m_littleEndian = true;

    Node         *m_parent   = nullptr;
    QVector<Node*> m_children;
};
