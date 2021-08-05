/***************************************************************************
 *   Copyright (C) 2021 by Julius Künzel (jk.kdedev@smartlab.uber.space)   *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "urllistparamwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "mltconnection.h"
#include "mainwindow.h"

UrlListParamWidget::UrlListParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

    // Get data from model
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    const QString configFile = m_model->data(m_index, AssetParameterModel::NewStuffRole).toString();

    // setup the comment
    setToolTip(comment);
    m_labelComment->setText(comment);
    m_widgetComment->setHidden(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_list->setIconSize(QSize(50, 30));
    setMinimumHeight(m_list->sizeHint().height());
    // setup download
    m_download->setHidden(configFile.isEmpty());
    // setup the name
    m_labelName->setText(m_model->data(m_index, Qt::DisplayRole).toString());
    slotRefresh();

    connect(m_download, &QToolButton::clicked, this, &UrlListParamWidget::downloadNewItems);
    connect(m_open, &QToolButton::clicked, this, &UrlListParamWidget::openFile);

    // emit the signal of the base class when appropriate
    // The connection is ugly because the signal "currentIndexChanged" is overloaded in QComboBox
    connect(this->m_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) {
                emit valueChanged(m_index, m_list->itemData(m_list->currentIndex()).toString(), true);
    });

}

void UrlListParamWidget::setCurrentIndex(int index)
{
    m_list->setCurrentIndex(index);
}

void UrlListParamWidget::setCurrentText(const QString &text)
{
    m_list->setCurrentText(text);
}

void UrlListParamWidget::addItem(const QString &text, const QVariant &value)
{
    m_list->addItem(text, value);
}

void UrlListParamWidget::setItemIcon(int index, const QIcon &icon)
{
    m_list->setItemIcon(index, icon);
}

void UrlListParamWidget::setIconSize(const QSize &size)
{
    m_list->setIconSize(size);
}

void UrlListParamWidget::slotShowComment(bool show)
{
    if (!m_labelComment->text().isEmpty()) {
        m_widgetComment->setVisible(show);
    }
}

QString UrlListParamWidget::getValue()
{
    return m_list->currentData().toString();
}

void UrlListParamWidget::slotRefresh()
{
    const QSignalBlocker bk(m_list);
    m_list->clear();
    QStringList names = m_model->data(m_index, AssetParameterModel::ListNamesRole).toStringList();
    QStringList values = m_model->data(m_index, AssetParameterModel::ListValuesRole).toStringList();
    QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    QString filter = m_model->data(m_index, AssetParameterModel::FilterRole).toString();
    filter.remove(0, filter.indexOf("(")+1);
    filter.remove(filter.indexOf(")")-1, -1);
    m_fileExt = filter.split(" ");

    if (values.first() == QLatin1String("%lumaPaths")) {
        // special case: Luma files
        values.clear();
        names.clear();
        if (pCore->getCurrentFrameSize().width() > 1000) {
            // HD project
            values = MainWindow::m_lumaFiles.value(QStringLiteral("16_9"));
        } else if (pCore->getCurrentFrameSize().height() > 1000) {
            values = MainWindow::m_lumaFiles.value(QStringLiteral("9_16"));
        } else if (pCore->getCurrentFrameSize().height() == pCore->getCurrentFrameSize().width()) {
            values = MainWindow::m_lumaFiles.value(QStringLiteral("square"));
        } else if (pCore->getCurrentFrameSize().height() == 480) {
            values = MainWindow::m_lumaFiles.value(QStringLiteral("NTSC"));
        } else {
            values = MainWindow::m_lumaFiles.value(QStringLiteral("PAL"));
        }
        m_list->addItem(i18n("None (Dissolve)"));
    }
    if (values.first() == QLatin1String("%lutPaths")) {
        // special case: LUT files
        values.clear();
        names.clear();
        // check for Kdenlive installed luts files

        QStringList customLuts = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("luts"), QStandardPaths::LocateDirectory);
#ifdef Q_OS_WIN
        // Windows downloaded lumas are saved in AppLocalDataLocation
        customLuts.append(QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("luts"), QStandardPaths::LocateDirectory));
#endif
        for (const QString &folderpath : qAsConst(customLuts)) {
            QDir dir(folderpath);
            QDirIterator it(dir.absolutePath(), m_fileExt, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                values.append(it.next());
            }
        }
    }
    // add all matching files in the location of the current item too
    if (!value.isEmpty()) {
        QString path = QUrl(value).adjusted(QUrl::RemoveFilename).toString();
        QDir dir(path);
        for (const auto &filename : dir.entryList(m_fileExt, QDir::Files)) {
            values.append(dir.filePath(filename));
        }
        // make sure the current value is added. If it is a duplicate we remove it later
        values << value;
    }

    values.removeDuplicates();

    // build ui list
    for (const QString &value : qAsConst(values)) {
        names.append(QUrl(value).fileName());
    }
    for (int i = 0; i < values.count(); i++) {
        const QString &entry = values.at(i);
        m_list->addItem(names.at(i), entry);
        // Create thumbnails
        if (!entry.isEmpty() && (entry.endsWith(QLatin1String(".png")) || entry.endsWith(QLatin1String(".pgm")))) {
            if (MainWindow::m_lumacache.contains(entry)) {
                m_list->setItemIcon(i + 1, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
            } else {
                QImage pix(entry);
                if (!pix.isNull()) {
                    MainWindow::m_lumacache.insert(entry, pix.scaled(50, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    m_list->setItemIcon(i + 1, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
                }
            }
        }
    }

    // select current value
    if (!value.isEmpty()) {
        int ix = m_list->findData(value);
        if (ix > -1)  {
            m_list->setCurrentIndex(ix);
        }
    }
}

void UrlListParamWidget::openFile()
{
    QString path = KRecentDirs::dir(QStringLiteral(":KdenliveUrlListParamFolder"));
    QString filter = m_model->data(m_index, AssetParameterModel::FilterRole).toString();

    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }

    QString urlString = QFileDialog::getOpenFileName(this, QString(), path, filter);

    if (!urlString.isEmpty()) {
        QString path = QUrl(urlString).adjusted(QUrl::RemoveFilename).toString();
        KRecentDirs::add(QStringLiteral(":KdenliveUrlListParamFolder"), QUrl(urlString).adjusted(QUrl::RemoveFilename).toString());
        emit valueChanged(m_index, urlString, true);
        slotRefresh();
    }
}

void UrlListParamWidget::downloadNewItems()
{
    const QString configFile = m_model->data(m_index, AssetParameterModel::NewStuffRole).toString();

    if(configFile.isEmpty()) {
        return;
    }

    if (pCore->getNewStuff(configFile) > 0) {
        if(configFile.contains(QStringLiteral("kdenlive_wipes.knsrc"))) {
            MltConnection::refreshLumas();
        }
        slotRefresh();
    }
}
