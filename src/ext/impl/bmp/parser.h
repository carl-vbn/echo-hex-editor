#pragma once

#include "ext/parser.h"

class BmpParser : public Parser {
public:
    BmpParser();
    NodeModel *parse(const Document &document) override;
    bool checkCompatibility(const Document &document) const override;
};
