#include "./parser.h"

#include <QDebug>
#include <QtEndian>

BmpParser::BmpParser() : Parser("BMP Image") {}

bool BmpParser::checkCompatibility(const Document &document) const
{
    if (document.size() < 2)
        return false;
    return document.read(0, 2) == "BM";
}

NodeModel *BmpParser::parse(const Document &document)
{
    // This is a very minimal parser that only reads the file header and
    // info header, and doesn't support all BMP variants.
    if (!checkCompatibility(document))
        return nullptr;

    NodeModel *model = new NodeModel;
    Node *root = model->createNode(nullptr, 0, document.size(), "BMP Image");

    auto readUInt32 = [&](qint64 offset) -> qint64 {
        return qFromLittleEndian<quint32>(document.read(offset, 4).constData());
    };
    auto readInt32 = [&](qint64 offset) -> qint64 {
        return qFromLittleEndian<qint32>(document.read(offset, 4).constData());
    };
    auto readUInt16 = [&](qint64 offset) -> qint64 {
        return qFromLittleEndian<quint16>(document.read(offset, 2).constData());
    };

    const qint64 fileSize     = readUInt32(2);
    const qint64 dataOffset   = readUInt32(10);
    const qint64 headerSize   = readUInt32(14);
    const qint64 bitsPerPixel = readUInt16(28);
    const qint64 compression  = headerSize >= 40 ? readUInt32(30) : 0;
    const qint64 colorsUsed   = headerSize >= 40 ? readUInt32(46) : 0;

    // --- File Header (14 bytes) ---
    model->createNode(root, 0,  2, "Magic",       Node::Type::UInt);
    model->createNode(root, 2,  4, "File Size",   Node::Type::UInt);
    model->createNode(root, 6,  4, "Reserved",    Node::Type::UInt);
    model->createNode(root, 10, 4, "Data Offset", Node::Type::UInt);

    // --- DIB / Info Header ---
    // infoHeader starts at document offset 14, so child offsets are relative to that.
    Node *infoHeader = model->createNode(root, 14, headerSize, "Info Header");
    model->createNode(infoHeader, 0,  4, "Header Size",        Node::Type::UInt);
    model->createNode(infoHeader, 4,  4, "Width",              Node::Type::Int);
    model->createNode(infoHeader, 8,  4, "Height",             Node::Type::Int);
    model->createNode(infoHeader, 12, 2, "Color Planes",       Node::Type::UInt);
    model->createNode(infoHeader, 14, 2, "Bits Per Pixel",     Node::Type::UInt);

    if (headerSize >= 40) {
        model->createNode(infoHeader, 16, 4, "Compression",        Node::Type::UInt);
        model->createNode(infoHeader, 20, 4, "Image Size",         Node::Type::UInt);
        model->createNode(infoHeader, 24, 4, "X Pixels Per Meter", Node::Type::Int);
        model->createNode(infoHeader, 28, 4, "Y Pixels Per Meter", Node::Type::Int);
        model->createNode(infoHeader, 32, 4, "Colors Used",        Node::Type::UInt);
        model->createNode(infoHeader, 36, 4, "Colors Important",   Node::Type::UInt);
    }

    if (headerSize >= 52) {
        model->createNode(infoHeader, 40, 4, "Red Mask",   Node::Type::UInt);
        model->createNode(infoHeader, 44, 4, "Green Mask", Node::Type::UInt);
        model->createNode(infoHeader, 48, 4, "Blue Mask",  Node::Type::UInt);
    }

    if (headerSize >= 56) {
        model->createNode(infoHeader, 52, 4, "Alpha Mask", Node::Type::UInt);
    }

    // Color Table
    qint64 colorTableEntries = 0;
    if (bitsPerPixel <= 8) {
        colorTableEntries = (colorsUsed > 0) ? colorsUsed : (1LL << bitsPerPixel);
    } else if (compression == 3 && colorsUsed > 0) {
        colorTableEntries = colorsUsed;
    }

    if (colorTableEntries > 0) {
        const qint64 colorTableSize   = colorTableEntries * 4;
        const qint64 colorTableOffset = dataOffset - colorTableSize;
        Node *colorTable = model->createNode(root, colorTableOffset, colorTableSize, "Color Table");
        for (qint64 i = 0; i < colorTableEntries; ++i) {
            model->createNode(colorTable, i * 4, 4,
                              QString("Color %1").arg(i), Node::Type::UInt);
        }
    }

    // Pixel Data
    const qint64 pixelDataSize = fileSize - dataOffset;
    if (pixelDataSize > 0 && dataOffset + pixelDataSize <= document.size()) {
        model->createNode(root, dataOffset, pixelDataSize, "Pixel Data", Node::Type::Blob);
    } else {
        qDebug() << "[BMP Parser] Warning: Invalid pixel data size or offset";
    }

    return model;
}