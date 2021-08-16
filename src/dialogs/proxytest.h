/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef PROXYTEST_H
#define PROXYTEST_H

#include "ui_testproxy_ui.h"

#include "definitions.h"
#include "timecode.h"
#include "timecodedisplay.h"

#include <QProcess>
#include <QMutex>

class MyTreeWidgetItem : public QTreeWidgetItem {
  public:
  MyTreeWidgetItem(QTreeWidget* parent, const QStringList &list):QTreeWidgetItem(parent, list){}
  MyTreeWidgetItem(QTreeWidgetItem* parent, const QStringList &list):QTreeWidgetItem(parent, list){}
  private:
  bool operator<(const QTreeWidgetItem &other)const override {
     int column = treeWidget()->sortColumn();
     if (column == 0) {
         // Sorting by name
         return text(column).toLower() < other.text(column).toLower();
     }
     return data(column, Qt::UserRole).toInt() < other.data(column, Qt::UserRole).toInt();
  }
};

/**
 * @class ProxyTest
 * @brief A dialog to compare the proxy profiles.
 * @author Jean-Baptiste Mardelle
 */
class ProxyTest : public QDialog, public Ui::TestProxy_UI
{
    Q_OBJECT

public:
    explicit ProxyTest(QWidget *parent = nullptr);
    ~ProxyTest() override;

private slots:
    void startTest();
    void addAnalysis(const QStringList &data);
    void showMessage(const QString &message);

private:
    bool m_closing;
    std::unique_ptr<QProcess> m_process;
    MyTreeWidgetItem *m_failedProfiles;
    QMutex m_locker;
};

#endif
