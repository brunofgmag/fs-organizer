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
    case LinkFailure::PrivilegeNotHeld:
        return QObject::tr("o Windows exige privilégio para criar link simbólico: ligue o Modo de Desenvolvedor, ou "
                           "volte o tipo de link para Junction nas Opções");
    case LinkFailure::PathIsNotAReparsePoint: return QObject::tr("o caminho não é um link, então nada foi removido");
    case LinkFailure::CouldNotRemoveLink: return QObject::tr("não foi possível remover o link");
    case LinkFailure::TheOutcomeIsUnknown:
        return QObject::tr("o diário registra esta operação, mas não diz como ela terminou");
    case LinkFailure::None: break;
    }

    return {};
}

QString Explain(const CategoryRule rule)
{
    switch (rule)
    {
    case CategoryRule::TheNameSaysAirport: return QObject::tr("o nome da pasta diz \"airport\"");
    case CategoryRule::TheNameSaysTraffic: return QObject::tr("o nome da pasta diz \"traffic\"");
    case CategoryRule::TheContentTypeIsScenery: return QObject::tr("o manifesto declara content_type SCENERY");
    case CategoryRule::TheContentTypeIsSound: return QObject::tr("o manifesto declara content_type SOUND");
    case CategoryRule::TheContentTypeIsLivery: return QObject::tr("o manifesto declara content_type LIVERY");
    case CategoryRule::None: break;
    }

    return {};
}

QString Explain(const FileResult result)
{
    switch (result)
    {
    case FileResult::Completed: return {};
    case FileResult::Cancelled: return QObject::tr("cancelada por você");
    case FileResult::TheSimulatorIsRunning: return QObject::tr("o simulador está em execução");
    case FileResult::CouldNotQuarantine:
        return QObject::tr("não foi possível mover a cópia perdedora para a quarentena");
    case FileResult::SourceIsNotUnderADestination:
        return QObject::tr("a pasta não está dentro de um destino do perfil");
    case FileResult::SourceIsAReparsePoint: return QObject::tr("a entrada é um link, e não uma pasta de verdade");
    case FileResult::CouldNotCheckFreeSpace:
        return QObject::tr("não foi possível consultar o espaço livre do volume de destino");
    case FileResult::NotEnoughFreeSpace: return QObject::tr("não há espaço livre suficiente na biblioteca");
    case FileResult::CouldNotCopy:
        return QObject::tr("a cópia falhou, e o que já foi copiado continua onde está para a retomada");
    case FileResult::VerificationFailed:
        return QObject::tr("a cópia não confere com a origem, então nada foi removido");
    case FileResult::CouldNotMoveIntoPlace: return QObject::tr("não foi possível pôr a cópia no lugar definitivo");
    case FileResult::CouldNotRemoveSource: return QObject::tr("não foi possível remover a pasta de origem");
    case FileResult::CouldNotCreateLink:
        return QObject::tr("os arquivos já estão na biblioteca, mas o link não pôde ser criado");
    case FileResult::TheOriginIsUnknown: return QObject::tr("o diário não sabe de onde isto veio");
    case FileResult::CouldNotRestore: return QObject::tr("já existe alguma coisa no lugar de origem");
    case FileResult::CouldNotDiscard: return QObject::tr("não foi possível descartar");
    case FileResult::CouldNotRemoveTheLink:
        return QObject::tr("não foi possível remover um dos links que apontam para a cópia da biblioteca");
    case FileResult::TheIdentityIsTaken: return QObject::tr("esta biblioteca já tem um addon com esse nome de pasta");
    case FileResult::TheTargetIsNotInALibrary:
        return QObject::tr("o destino da operação não está dentro de uma biblioteca do perfil");
    case FileResult::CouldNotCreateTheCategory: return QObject::tr("não foi possível criar a categoria");
    case FileResult::TheCategoryStillHoldsAddons:
        return QObject::tr("esta categoria ainda guarda addons, e só categoria vazia pode ser apagada");
    case FileResult::CouldNotRemoveTheCategory: return QObject::tr("não foi possível apagar a categoria");
    case FileResult::TheOutcomeIsUnknown:
        return QObject::tr("o diário registra esta operação, mas não diz como ela terminou");
    case FileResult::CouldNotReadTheSource:
        return QObject::tr("não foi possível percorrer a pasta de origem, então nada foi copiado");
    }

    return {};
}

namespace
{
    QString WhereTheOccupantIs(const std::filesystem::path& occupant)
    {
        return occupant.empty() ? QString{} : QObject::tr("\n    o ocupante está em: %1").arg(AsText(occupant));
    }
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

QString Describe(const ImportOperationResult& result)
{
    return QStringLiteral("%1 — %2%3")
        .arg(AsText(result.request.source.filename()), Explain(result.result), WhereTheOccupantIs(result.occupant));
}

QString Describe(const FileOperationResult& result)
{
    return QStringLiteral("%1 — %2%3")
        .arg(AsText(result.path.filename()), Explain(result.result), WhereTheOccupantIs(result.occupant));
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
