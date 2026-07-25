#include "view/FailureText.h"

#include <QtCore/QCoreApplication>

namespace
{
    QString Show(const std::filesystem::path& path)
    {
        return QString::fromStdWString(path.wstring());
    }
}

QString Explain(const LinkFailure failure)
{
    switch (failure)
    {
    case LinkFailure::DestinationHoldsRealFolder:
        return QObject::tr("já existe uma pasta de verdade com esse nome no destino");
    case LinkFailure::DestinationHoldsLiveLink:
        return QObject::tr("o destino já tem um link vivo de outro programa");
    case LinkFailure::UnreadableLinkTarget:
        return QObject::tr("não foi possível ler o alvo do link que ocupa o destino");
    case LinkFailure::CouldNotReplaceStaleLink:
        return QObject::tr("não foi possível remover o link morto que ocupava o destino");
    case LinkFailure::CouldNotCreateLink:
        return QObject::tr("não foi possível criar o link");
    case LinkFailure::PathIsNotAReparsePoint:
        return QObject::tr("o caminho não é um link, então nada foi removido");
    case LinkFailure::CouldNotRemoveLink:
        return QObject::tr("não foi possível remover o link");
    case LinkFailure::None:
        break;
    }

    return {};
}

QString Describe(const LinkOperationResult& result)
{
    QString line = QStringLiteral("%1 — %2")
        .arg(Show(result.addonFolder.filename()), Explain(result.outcome.Failure()));

    if (result.outcome.Conflict().has_value())
    {
        line += QObject::tr("\n    pasta no destino: %1\n    addon na biblioteca: %2")
            .arg(Show(result.outcome.Conflict()->destinationPath),
                 Show(result.outcome.Conflict()->libraryPath));
    }

    if (result.outcome.Occupation().has_value())
    {
        line += QObject::tr("\n    o link atual aponta para: %1")
            .arg(Show(result.outcome.Occupation()->existingTarget));
    }

    return line;
}
