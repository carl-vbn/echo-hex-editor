#include "centerpanel.h"
#include "ui/hexview.h"
#include "core/document.h"
#include "theme/theme.h"

#include <QVBoxLayout>

CenterPanel::CenterPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void CenterPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_document = new Document(this);
    m_document->loadData(makeTestData());

    m_hexView = new HexView(this);
    m_hexView->setDocument(m_document);
    layout->addWidget(m_hexView);
}

// Test data
//
// A synthetic buffer that exercises all visual states:
//   - ASCII text  → readable characters in the ASCII column
//   - Zero runs   → very dim bytes, easy to spot
//   - Binary data → incrementing / patterned values
//   - High bytes  → 0x80–0xFF, shown as · in ASCII

QByteArray CenterPanel::makeTestData()
{
    QByteArray d;

    // Fake ELF-style magic + header
    d += QByteArray("\x7F\x45\x4C\x46\x02\x01\x01\x00", 8); // magic, 64-bit LE
    d += QByteArray("\x00\x00\x00\x00\x00\x00\x00\x00", 8); // padding
    d += QByteArray("\x02\x00\x3E\x00\x01\x00\x00\x00", 8); // e_type, e_machine
    d += QByteArray("\x40\x10\x40\x00\x00\x00\x00\x00", 8); // e_entry

    // ASCII text sections
    d += "ECHO TEST BUFFER\x00";
    d += "The quick brown fox jumps over the lazy dog.\x00";
    d += "ABCDEFGHIJKLMNOPQRSTUVWXYZ\x00";
    d += "abcdefghijklmnopqrstuvwxyz\x00";
    d += "0123456789 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x00";

    // Zero run
    d += QByteArray(48, '\x00');

    // Full byte range 0x00–0xFF
    for (int i = 0; i < 256; ++i)
        d += static_cast<char>(i);

    // Alternating 0x55 / 0xAA
    for (int i = 0; i < 64; ++i)
        d += static_cast<char>((i & 1) ? 0xAA : 0x55);

    // Fibonacci mod 256
    int a = 1, b = 1;
    for (int i = 0; i < 64; ++i) {
        d += static_cast<char>(a & 0xFF);
        const int c = (a + b) & 0xFF;
        a = b; b = c;
    }

    // All 0xFF
    d += QByteArray(32, '\xFF');

    return d;
}
