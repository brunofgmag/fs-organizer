#ifndef FS_ORGANIZER_TESTS_SUPPORT_PAGE_FLOOR_H
#define FS_ORGANIZER_TESTS_SUPPORT_PAGE_FLOOR_H

#include <algorithm>
#include <vector>

#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QtWidgets/QLayout>
#include <QtWidgets/QWidget>

#include "view/theme/ModernistTheme.h"

inline constexpr int kWidestAPageMayBe = 1024;
inline constexpr int kHowManyCulpritsToName = 6;

inline QString WhatIsHoldingItOpen(const QWidget& page)
{
    std::vector<std::pair<int, QString>> asking;

    for (const QWidget* child : page.findChildren<QWidget*>())
    {
        const int hard = std::max(child->minimumWidth(), child->minimumSizeHint().width());

        if (hard >= kWidestAPageMayBe / 4)
        {
            const QString name = child->objectName().isEmpty()
                ? QString::fromLatin1(child->metaObject()->className())
                : QStringLiteral("%1 %2").arg(QString::fromLatin1(child->metaObject()->className()),
                                              child->objectName());
            asking.emplace_back(hard, name);
        }
    }

    std::ranges::sort(asking, std::ranges::greater{}, &std::pair<int, QString>::first);
    asking.resize(std::min<std::size_t>(asking.size(), kHowManyCulpritsToName));

    QStringList said;
    for (const auto& [hard, name] : asking)
    {
        said << QStringLiteral("%1 asks %2").arg(name).arg(hard);
    }

    return said.join(QStringLiteral(", "));
}

inline void ItFitsTheNarrowestWindow(QWidget& page, const char* named)
{
    ApplyModernistTheme(*qApp);

    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QCoreApplication::processEvents();

    const int asked = page.minimumSizeHint().width();

    const QString said = QStringLiteral("%1 will not shrink below %2 px, and no page may refuse to go under %3, "
                                        "which is what fits a 1080 px screen. %4")
                             .arg(QLatin1String(named))
                             .arg(asked)
                             .arg(kWidestAPageMayBe)
                             .arg(WhatIsHoldingItOpen(page));

    QVERIFY2(asked <= kWidestAPageMayBe, qPrintable(said));
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_PAGE_FLOOR_H
