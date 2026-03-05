#include "nodeutil.h"
#include "node.h"
#include "document.h"

#include <cstring>  // memcpy

QString interpretNodeValue(Node *node, Document *doc)
{
    if (!doc || !node || node->type() == Node::Type::Blob || node->isRoot())
        return {};

    const int size = static_cast<int>(node->length());
    if (size <= 0 || size > 8)
        return {};

    const QByteArray bytes = doc->read(node->absoluteStart(), size);
    if (bytes.size() < size) return {};

    const bool le = node->isLittleEndian();

    auto readU = [&]() -> quint64 {
        quint64 result = 0;
        if (le) {
            for (int i = size - 1; i >= 0; --i)
                result = (result << 8) | static_cast<uint8_t>(bytes.at(i));
        } else {
            for (int i = 0; i < size; ++i)
                result = (result << 8) | static_cast<uint8_t>(bytes.at(i));
        }
        return result;
    };

    switch (node->type()) {
    case Node::Type::UInt:
        return QString::number(readU());
    case Node::Type::Int: {
        const quint64 raw = readU();
        const int bits = size * 8;
        const quint64 signBit = quint64(1) << (bits - 1);
        const qint64 signed_ = (raw & signBit)
            ? static_cast<qint64>(raw | (~quint64(0) << bits))
            : static_cast<qint64>(raw);
        return QString::number(signed_);
    }
    case Node::Type::Float: {
        if (size == 4) {
            quint32 bits = static_cast<quint32>(readU());
            float f; std::memcpy(&f, &bits, sizeof f);
            return QString::number(static_cast<double>(f), 'g', 7);
        } else if (size == 8) {
            quint64 bits = readU();
            double d; std::memcpy(&d, &bits, sizeof d);
            return QString::number(d, 'g', 15);
        }
        return {};
    }
    default: return {};
    }
}

bool writeNodeValue(Node *node, Document *doc, const QString &text)
{
    if (!doc || !node || node->type() == Node::Type::Blob || node->isRoot())
        return false;

    const int size = static_cast<int>(node->length());
    if (size <= 0 || size > 8)
        return false;

    const bool le = node->isLittleEndian();
    quint64 raw = 0;

    switch (node->type()) {
    case Node::Type::UInt: {
        bool ok;
        raw = text.toULongLong(&ok);
        if (!ok) return false;
        break;
    }
    case Node::Type::Int: {
        bool ok;
        qint64 signed_ = text.toLongLong(&ok);
        if (!ok) return false;
        std::memcpy(&raw, &signed_, sizeof raw);
        break;
    }
    case Node::Type::Float: {
        bool ok;
        if (size == 4) {
            float f = text.toFloat(&ok);
            if (!ok) return false;
            quint32 bits;
            std::memcpy(&bits, &f, sizeof bits);
            raw = bits;
        } else if (size == 8) {
            double d = text.toDouble(&ok);
            if (!ok) return false;
            std::memcpy(&raw, &d, sizeof raw);
        } else {
            return false;
        }
        break;
    }
    default:
        return false;
    }

    // Convert raw value to bytes respecting endianness
    QByteArray bytes(size, '\0');
    if (le) {
        for (int i = 0; i < size; ++i) {
            bytes[i] = static_cast<char>(raw & 0xFF);
            raw >>= 8;
        }
    } else {
        for (int i = size - 1; i >= 0; --i) {
            bytes[i] = static_cast<char>(raw & 0xFF);
            raw >>= 8;
        }
    }

    doc->overwrite(node->absoluteStart(), bytes);
    return true;
}
