/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *               2013 by Jean-Nicolas Artaud (jeannicolasartaud@gmail.com) *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

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
