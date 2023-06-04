#include "qstringutils.h"

#include <QString>
#include <QStringList>

QString QStringUtils::getUniqueName(const QStringList &names, const QString &name)
{
    int i = 0;
    QString newName = name;
    while (names.contains(name)) {
        // name is not unique, add a suffix
        newName = name + QString("-%1").arg(i);
        i++;
    }
    return newName;
}

QString QStringUtils::appendToFilename(const QString &filename, const QString &appendix)
{
    QString name = filename.section(QLatin1Char('.'), 0, -2);
    QString extension = filename.section(QLatin1Char('.'), -1);
    return name + appendix + extension;
}
