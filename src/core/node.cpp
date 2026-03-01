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
    case Type::Blob: return "Blob";
    case Type::U8:   return "U8";
    case Type::I8:   return "I8";
    case Type::U16:  return "U16";
    case Type::I16:  return "I16";
    case Type::U32:  return "U32";
    case Type::I32:  return "I32";
    case Type::U64:  return "U64";
    case Type::I64:  return "I64";
    case Type::F32:  return "F32";
    case Type::F64:  return "F64";
    }
    return {};
}

QStringList Node::allTypeNames()
{
    return { "Blob", "U8", "I8", "U16", "I16", "U32", "I32", "U64", "I64", "F32", "F64" };
}

Node::Type Node::typeFromName(const QString &name)
{
    if (name == "U8")  return Type::U8;
    if (name == "I8")  return Type::I8;
    if (name == "U16") return Type::U16;
    if (name == "I16") return Type::I16;
    if (name == "U32") return Type::U32;
    if (name == "I32") return Type::I32;
    if (name == "U64") return Type::U64;
    if (name == "I64") return Type::I64;
    if (name == "F32") return Type::F32;
    if (name == "F64") return Type::F64;
    return Type::Blob;
}

int Node::typeSize(Type t)
{
    switch (t) {
    case Type::Blob:              return 0;
    case Type::U8:  case Type::I8:   return 1;
    case Type::U16: case Type::I16:  return 2;
    case Type::U32: case Type::I32:  return 4;
    case Type::U64: case Type::I64:  return 8;
    case Type::F32:               return 4;
    case Type::F64:               return 8;
    }
    return 0;
}
