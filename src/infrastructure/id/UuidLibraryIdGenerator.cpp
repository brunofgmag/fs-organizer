#include "infrastructure/id/UuidLibraryIdGenerator.h"

#include <QtCore/QUuid>

LibraryId UuidLibraryIdGenerator::Generate() const
{
    return QUuid::createUuid().toString(QUuid::WithBraces).toStdString();
}
