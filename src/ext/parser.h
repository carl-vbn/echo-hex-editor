#pragma once

#include "core/document.h"
#include "core/nodemodel.h"
#include <QString>

class Parser {
public:
    Parser(const QString &name) : m_name(name) {}

    QString name() const { return m_name; }

    virtual bool checkCompatibility(const Document &document) const = 0;

    // Parse the document and return the root node.  The returned model
    // must be owned by the caller.
    virtual NodeModel *parse(const Document &document) = 0;
private:
    QString m_name;
};