#pragma once
#include <QString>

class Node;
class Document;
class NodeModel;

// Interpret a typed leaf node's bytes as a human-readable value string.
// For Reference nodes, returns the stored value formatted as hex ("0x...").
// Returns an empty string for Blob nodes, root nodes, or sizes > 8.
QString interpretNodeValue(Node *node, Document *doc);

// Parse a value string and write the corresponding bytes back to the document.
// Returns true on success.
bool writeNodeValue(Node *node, Document *doc, const QString &text);

// Resolved info for a Reference-type node.
struct RefInfo {
    bool   valid    = false;
    qint64 absolute = -1;  // base_node.absoluteStart + storedValue + constantOffset
    qint64 relative = -1;  // storedValue + constantOffset (offset from base node start)
};

// Resolve a Reference node's stored value against its base node.
// Returns an invalid RefInfo if the node is not a Reference or data is unavailable.
RefInfo resolveNodeReference(Node *node, Document *doc, NodeModel *model);
