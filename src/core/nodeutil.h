#pragma once
#include <QString>

class Node;
class Document;

// Interpret a typed leaf node's bytes as a human-readable value string.
// Returns an empty string for Blob nodes, root nodes, or sizes > 8.
QString interpretNodeValue(Node *node, Document *doc);

// Parse a value string and write the corresponding bytes back to the document.
// Returns true on success.
bool writeNodeValue(Node *node, Document *doc, const QString &text);
