#include <QtTest/QtTest>

#include "tests/support/EnumPrinting.h"

class EnumPrintingTest : public QObject
{
    Q_OBJECT

private slots:
    static void EveryFileResultPrintsItsOwnName();
    static void EveryOperationKindPrintsItsOwnName();
};

namespace
{
    template<typename Value>
    QString NameOf(const Value value)
    {
        char* printed = QTest::toString(value);
        const QString name = QString::fromLatin1(printed);
        delete[] printed;

        return name;
    }

    template<typename Value>
    void EveryValueIsNamed(const auto& values, const QString& family)
    {
        for (const Value value : values)
        {
            const QString name = NameOf(value);

            QVERIFY2(
                !name.contains(QLatin1Char('?')),
                qPrintable(
                    QStringLiteral("%1 %2 has no name in EnumPrinting.h").arg(family).arg(static_cast<int>(value))));
        }
    }
}

void EnumPrintingTest::EveryFileResultPrintsItsOwnName()
{
    EveryValueIsNamed<FileResult>(kAllFileResults, QStringLiteral("FileResult"));
}

void EnumPrintingTest::EveryOperationKindPrintsItsOwnName()
{
    EveryValueIsNamed<OperationKind>(kAllOperationKinds, QStringLiteral("OperationKind"));
}

QTEST_APPLESS_MAIN(EnumPrintingTest)

#include "tst_enum_printing.moc"
