#include "viewmodel/FailureText.h"

#include <QtCore/QCoreApplication>

#include "support/PathText.h"

QString Explain(const LinkFailure failure)
{
    switch (failure)
    {
    case LinkFailure::DestinationHoldsRealFolder:
        return QObject::tr("já existe uma pasta de verdade com esse nome no destino");
    case LinkFailure::DestinationHoldsLiveLink: return QObject::tr("o destino já tem um link vivo de outro programa");
    case LinkFailure::UnreadableLinkTarget:
        return QObject::tr("não foi possível ler o alvo do link que ocupa o destino");
    case LinkFailure::CouldNotReplaceStaleLink:
        return QObject::tr("não foi possível remover o link morto que ocupava o destino");
    case LinkFailure::CouldNotCreateLink: return QObject::tr("não foi possível criar o link");
    case LinkFailure::PathIsNotAReparsePoint: return QObject::tr("o caminho não é um link, então nada foi removido");
    case LinkFailure::CouldNotRemoveLink: return QObject::tr("não foi possível remover o link");
    case LinkFailure::None: break;
    }

    return {};
}

QString Explain(const ImportResult result)
{
    switch (result)
    {
    case ImportResult::Completed: return {};
    case ImportResult::Cancelled: return QObject::tr("cancelada por você");
    case ImportResult::TheSimulatorIsRunning: return QObject::tr("o simulador está em execução");
    case ImportResult::CouldNotQuarantine:
        return QObject::tr("não foi possível mover a cópia perdedora para a quarentena");
    case ImportResult::SourceIsNotUnderADestination:
        return QObject::tr("a pasta não está dentro de um destino do perfil");
    case ImportResult::SourceIsAReparsePoint: return QObject::tr("a entrada é um link, e não uma pasta de verdade");
    case ImportResult::CouldNotCheckFreeSpace:
        return QObject::tr("não foi possível consultar o espaço livre do volume de destino");
    case ImportResult::NotEnoughFreeSpace: return QObject::tr("não há espaço livre suficiente na biblioteca");
    case ImportResult::CouldNotCopy:
        return QObject::tr("a cópia falhou, e o que já foi copiado continua onde está para a retomada");
    case ImportResult::VerificationFailed:
        return QObject::tr("a cópia não confere com a origem, então nada foi removido");
    case ImportResult::CouldNotMoveIntoPlace: return QObject::tr("não foi possível pôr a cópia no lugar definitivo");
    case ImportResult::CouldNotRemoveSource: return QObject::tr("não foi possível remover a pasta de origem");
    case ImportResult::CouldNotCreateLink:
        return QObject::tr("os arquivos já estão na biblioteca, mas o link não pôde ser criado");
    case ImportResult::TheOriginIsUnknown: return QObject::tr("o diário não sabe de onde isto veio");
    case ImportResult::CouldNotRestore: return QObject::tr("já existe alguma coisa no lugar de origem");
    case ImportResult::CouldNotDiscard: return QObject::tr("não foi possível descartar");
    case ImportResult::CouldNotRemoveTheLink:
        return QObject::tr("não foi possível remover um dos links que apontam para a cópia da biblioteca");
    }

    return {};
}

QString Describe(const LinkOperationResult& result)
{
    QString line =
        QStringLiteral("%1 — %2").arg(AsText(result.addonFolder.filename()), Explain(result.outcome.Failure()));

    if (result.outcome.Conflict().has_value())
    {
        line += QObject::tr("\n    pasta no destino: %1\n    addon na biblioteca: %2")
                    .arg(AsText(result.outcome.Conflict()->destinationPath),
                         AsText(result.outcome.Conflict()->libraryPath));
    }

    if (result.outcome.Occupation().has_value())
    {
        line +=
            QObject::tr("\n    o link atual aponta para: %1").arg(AsText(result.outcome.Occupation()->existingTarget));
    }

    return line;
}

QString NameOfImportStep(const OperationKind kind)
{
    switch (kind)
    {
    case OperationKind::ImportCopyToStaging: return QObject::tr("Copiando para a biblioteca...");
    case OperationKind::ImportVerifyStaging: return QObject::tr("Conferindo se a cópia bate com a origem...");
    case OperationKind::ImportMoveIntoPlace: return QObject::tr("Pondo a cópia no lugar definitivo...");
    case OperationKind::ImportRemoveSource: return QObject::tr("Removendo a pasta de origem...");
    case OperationKind::EnableAddon: return QObject::tr("Criando o link no destino...");
    default: return {};
    }
}
