#pragma once
#include <QWidget>
#include <array>

class QLabel;
class QGridLayout;
class QButtonGroup;
class Document;

class RightPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RightPanel(QWidget *parent = nullptr);

    void setDocument(Document *doc);

public slots:
    void onSelectionChanged(qint64 start, qint64 end);

private:
    Document     *m_document    = nullptr;
    QButtonGroup *m_endianGroup = nullptr;

    // Selection info
    QLabel *m_offsetLabel = nullptr;
    QLabel *m_lengthLabel = nullptr;

    // Type interpretation value labels (U8 I8 U16 I16 U32 I32 U64 I64 F32 F64)
    static constexpr int TYPE_COUNT = 10;
    std::array<QLabel *, TYPE_COUNT> m_typeValues{};

    qint64 m_selStart = -1;
    qint64 m_selEnd   = -1;

    void setupUi();
    void updateInterpretations();

    static QWidget *makePanelHeader(const QString &text, QWidget *parent = nullptr);
    static QLabel  *addFieldRow(QGridLayout *grid, int row, const QString &label);
};
