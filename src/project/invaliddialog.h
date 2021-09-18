/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2013 Jean-Nicolas Artaud <jeannicolasartaud@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef INVALIDDIALOG_H
#define INVALIDDIALOG_H

#include <QDialog>

class QListWidget;

class InvalidDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InvalidDialog(const QString &caption, const QString &message, bool infoOnly, QWidget *parent = nullptr);
    ~InvalidDialog() override;

    void addClip(const QString &id, const QString &path);
    QStringList getIds() const;

private:
    QListWidget *m_clipList;
};

#endif // INVALIDDIALOG_H
