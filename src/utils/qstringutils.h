#pragma once

class QString;
class QStringList;

class QStringUtils
{
public:
    static QString getUniqueName(const QStringList &names, const QString &name);
    /** Append @param appendix to @param filename before the file extension.
     *  The part of the string after the last dot will be treated as file extension.
     */
    static QString appendToFilename(const QString &filename, const QString &appendix);
};
