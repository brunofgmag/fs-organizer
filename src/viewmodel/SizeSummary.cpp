#include "viewmodel/SizeSummary.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>

namespace
{
    [[nodiscard]] QString NotMeasured(const UnmeasuredEntries& group)
    {
        const auto many = static_cast<int>(group.count);

        switch (group.classification)
        {
        case EntryClassification::Managed:
            return QCoreApplication::translate("SizeSummary", "%n managed not measured", nullptr, many);
        case EntryClassification::External:
            return QCoreApplication::translate("SizeSummary", "%n external not measured", nullptr, many);
        case EntryClassification::Broken:
            return QCoreApplication::translate("SizeSummary", "%n broken not measured", nullptr, many);
        case EntryClassification::Unavailable:
            return QCoreApplication::translate("SizeSummary", "%n unavailable not measured", nullptr, many);
        case EntryClassification::Unmanaged:
            return QCoreApplication::translate("SizeSummary", "%n unmanaged not measured", nullptr, many);
        case EntryClassification::Duplicated:
            return QCoreApplication::translate("SizeSummary", "%n duplicated not measured", nullptr, many);
        }

        return {};
    }

    [[nodiscard]] QString WhatWasLeftOut(const std::vector<UnmeasuredEntries>& unmeasured)
    {
        QStringList reasons;
        for (const UnmeasuredEntries& group : unmeasured)
        {
            reasons.append(NotMeasured(group));
        }

        return reasons.join(QCoreApplication::translate("SizeSummary", ", "));
    }
}

QString SizeOfTheSelection(const SelectionSize& size)
{
    if (size.selected == 0)
    {
        return {};
    }

    const QString bytes = AsSize(size.bytes);

    if (size.measured == size.selected && size.unmeasured.empty())
    {
        return bytes;
    }

    const auto measured = static_cast<int>(size.measured);
    const QString reach = QCoreApplication::translate("SizeSummary", "%1 across %n of %2 selected", nullptr, measured)
                              .arg(bytes)
                              .arg(size.selected);

    if (size.unmeasured.empty())
    {
        return reach;
    }

    return QStringLiteral("%1 · %2").arg(reach, WhatWasLeftOut(size.unmeasured));
}
