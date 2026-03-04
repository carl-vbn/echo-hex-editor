#include "node.h"

Node::Node(quint64 id, Node *parent, qint64 startOffset, qint64 length, const QString &name)
    : m_id(id), m_parent(parent), m_startOffset(startOffset), m_length(length), m_name(name)
{}

Node::~Node()
{
    for (Node *child : m_children)
        delete child;
}

qint64 Node::absoluteStart() const
{
    if (!m_parent) return m_startOffset;
    return m_parent->absoluteStart() + m_startOffset;
}

QString Node::typeName(Type t)
{
    switch (t) {
    case Type::Blob:  return "Blob";
    case Type::UInt:  return "UInt";
    case Type::Int:   return "Int";
    case Type::Float: return "Float";
    }
    return {};
}

QStringList Node::allTypeNames()
{
    return { "Blob", "UInt", "Int", "Float" };
}

Node::Type Node::typeFromName(const QString &name)
{
    if (name == "UInt")  return Type::UInt;
    if (name == "Int")   return Type::Int;
    if (name == "Float") return Type::Float;
    return Type::Blob;
}
