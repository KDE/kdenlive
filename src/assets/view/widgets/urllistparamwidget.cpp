/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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

    // emit the signal of the base class when appropriate
    // The connection is ugly because the signal "currentIndexChanged" is overloaded in QComboBox
    connect(this->m_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this](int index) {
        if (m_list->currentData() == QStringLiteral("custom_file")) {
            openFile();
        } else {
            m_currentIndex = index;
            emit valueChanged(m_index, m_list->currentData().toString(), true);
        }
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
    QString currentValue = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    QString filter = m_model->data(m_index, AssetParameterModel::FilterRole).toString();
    filter.remove(0, filter.indexOf("(")+1);
    filter.remove(filter.indexOf(")")-1, -1);
    m_fileExt = filter.split(" ");
    if (!values.isEmpty() && values.first() == QLatin1String("%lumaPaths")) {
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
    if (!values.isEmpty() && values.first() == QLatin1String("%lutPaths")) {
        // special case: LUT files
        values.clear();
        names.clear();

        // check for Kdenlive installed luts files
        QStringList customLuts = QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("luts"), QStandardPaths::LocateDirectory);
        for (const QString &folderpath : qAsConst(customLuts)) {
            QDir dir(folderpath);
            QDirIterator it(dir.absolutePath(), m_fileExt, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                values.append(it.next());
            }
        }
    }
    // add all matching files in the location of the current item too
    if (!currentValue.isEmpty()) {
        QString path = QUrl(currentValue).adjusted(QUrl::RemoveFilename).toString();
        QDir dir(path);
        for (const auto &filename : dir.entryList(m_fileExt, QDir::Files)) {
            values.append(dir.filePath(filename));
        }
        // make sure the current value is added. If it is a duplicate we remove it later
        values << currentValue;
    }

    values.removeDuplicates();

    // build ui list
    for (const QString &value : qAsConst(values)) {
        names.append(pCore->nameForLumaFile(QUrl(value).fileName()));
    }
    for (int i = 0; i < values.count(); i++) {
        const QString &entry = values.at(i);
        QString name = QFileInfo(names.at(i)).baseName();
        m_list->addItem(name, entry);
        int ix = m_list->findData(entry);
        // Create thumbnails
        if (!entry.isEmpty() && (entry.endsWith(QLatin1String(".png")) || entry.endsWith(QLatin1String(".pgm")))) {
            if (MainWindow::m_lumacache.contains(entry)) {
                m_list->setItemIcon(ix, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
            } else {
                QImage pix(entry);
                if (!pix.isNull()) {
                    MainWindow::m_lumacache.insert(entry, pix.scaled(50, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    m_list->setItemIcon(ix, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
                }
            }
        }
    }
    m_list->addItem(i18n("Custom…"), QStringLiteral("custom_file"));

    // select current value
    if (!currentValue.isEmpty()) {
        int ix = m_list->findData(currentValue);
        if (ix > -1)  {
            m_list->setCurrentIndex(ix);
            m_currentIndex = ix;
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
        KRecentDirs::add(QStringLiteral(":KdenliveUrlListParamFolder"), QUrl(urlString).adjusted(QUrl::RemoveFilename).toString());
        emit valueChanged(m_index, urlString, true);
        slotRefresh();
    } else {
        m_list->setCurrentIndex(m_currentIndex);
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
