/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "urllistparamwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "mltconnection.h"

#include <QDirIterator>
#include <QFileDialog>
#include <QtConcurrent>

UrlListParamWidget::UrlListParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_isLumaList(false)
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
    if (!configFile.isEmpty()) {
        m_knsbutton->setConfigFile(configFile);
        connect(m_knsbutton, &KNSWidgets::Button::dialogFinished, this, [this, configFile](const QList<KNSCore::Entry> &changedEntries) {
            if (changedEntries.count() > 0) {
                if (configFile.contains(QStringLiteral("kdenlive_wipes.knsrc"))) {
                    MltConnection::refreshLumas();
                }
                slotRefresh();
            }
        });
    } else {
        m_knsbutton->hide();
    }
    // setup the name
    m_labelName->setText(m_model->data(m_index, Qt::DisplayRole).toString());
    m_isLutList = m_model->getAssetId().startsWith(QLatin1String("avfilter.lut3d"));
    UrlListParamWidget::slotRefresh();

    // Q_EMIT the signal of the base class when appropriate
    // The connection is ugly because the signal "currentIndexChanged" is overloaded in QComboBox
    connect(this->m_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this](int index) {
        if (m_list->currentData() == QStringLiteral("custom_file")) {
            openFile();
        } else {
            m_currentIndex = index;
            Q_EMIT valueChanged(m_index, m_list->currentData().toString(), true);
        }
    });
}

UrlListParamWidget::~UrlListParamWidget()
{
    if (m_watcher.isRunning()) {
        m_abortJobs = true;
        m_watcher.waitForFinished();
    }
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
    filter.remove(0, filter.indexOf("(") + 1);
    filter.remove(filter.indexOf(")") - 1, -1);
    m_fileExt = filter.split(" ");
    if (!values.isEmpty() && values.first() == QLatin1String("%lumaPaths")) {
        // special case: Luma files
        m_isLumaList = true;
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
        const QString path = QUrl(currentValue).adjusted(QUrl::RemoveFilename).toString();
        QDir dir(path);
        if (dir.exists()) {
            QStringList entrys = dir.entryList(m_fileExt, QDir::Files);
            for (const auto &filename : qAsConst(entrys)) {
                values.append(dir.filePath(filename));
            }
            // make sure the current value is added. If it is a duplicate we remove it later
            if (QFileInfo::exists(currentValue)) {
                values << currentValue;
            }
        }
    }

    values.removeDuplicates();

    // build ui list
    QMap<QString, QString> entryMap;
    int ix = 0;
    // Put all name/value combinations in a map
    for (const QString &value : qAsConst(values)) {
        if (m_isLutList) {
            if (value.toLower().endsWith(QLatin1String(".cube")) && !KdenliveSettings::validated_luts().contains(value)) {
                // Open LUT file and check validity
                if (isValidCubeFile(value)) {
                    entryMap.insert(QFileInfo(value).baseName(), value);
                    QStringList validated = KdenliveSettings::validated_luts();
                    validated << value;
                    KdenliveSettings::setValidated_luts(validated);
                } else {
                    qDebug() << ":::: FOUND INVALID LUT FILE: " << value;
                }
            } else {
                entryMap.insert(QFileInfo(value).baseName(), value);
            }
        } else if (m_isLumaList) {
            QString lumaName = pCore->nameForLumaFile(QFileInfo(value).fileName());
            if (entryMap.contains(lumaName)) {
                // Duplicate name, add a suffix
                const QString baseName = lumaName;
                int i = 2;
                lumaName = baseName + QString(" / %1").arg(i);
                while (entryMap.contains(lumaName)) {
                    i++;
                    lumaName = baseName + QString(" / %1").arg(i);
                }
            }
            entryMap.insert(lumaName, value);
        } else if (ix < names.count()) {
            entryMap.insert(names.at(ix), value);
        }
        ix++;
    }
    QMapIterator<QString, QString> i(entryMap);
    QStringList thumbnailsToBuild;
    while (i.hasNext()) {
        i.next();
        const QString &entry = i.value();
        m_list->addItem(i.key(), entry);
        int ix = m_list->findData(entry);
        // Create thumbnails
        if (!entry.isEmpty() && (entry.toLower().endsWith(QLatin1String(".png")) || entry.toLower().endsWith(QLatin1String(".pgm")))) {
            if (MainWindow::m_lumacache.contains(entry)) {
                m_list->setItemIcon(ix, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
            } else {
                // render thumbnails in another thread
                thumbnailsToBuild << entry;
            }
        }
    }
    m_list->addItem(i18n("Custom…"), QStringLiteral("custom_file"));

    // select current value
    if (!currentValue.isEmpty()) {
        int ix = m_list->findData(currentValue);
        if (ix > -1) {
            m_list->setCurrentIndex(ix);
            m_currentIndex = ix;
        } else {
            // If the project file references a luma file in the Appimage, but the widget lists system installed luma files, try harder to find a match
            if (currentValue.startsWith(QStringLiteral("/tmp/.mount_"))) {
                const QString endPath = currentValue.section(QLatin1Char('/'), -2);
                // Parse all entries to see if we have a matching filename
                for (int j = 0; j < m_list->count(); j++) {
                    if (m_list->itemData(j, Qt::UserRole).toString().endsWith(endPath)) {
                        m_list->setCurrentIndex(j);
                        m_currentIndex = j;
                        break;
                    }
                }
            }
        }
    }
    if (!thumbnailsToBuild.isEmpty() && !m_watcher.isRunning()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_thumbJob = QtConcurrent::run(this, &UrlListParamWidget::buildThumbnails, thumbnailsToBuild);
#else
        m_thumbJob = QtConcurrent::run(&UrlListParamWidget::buildThumbnails, this, thumbnailsToBuild);
#endif
        m_watcher.setFuture(m_thumbJob);
    }
}

void UrlListParamWidget::buildThumbnails(const QStringList files)
{
    for (auto &f : files) {
        QImage pix(f);
        if (m_abortJobs) {
            break;
        }
        if (!pix.isNull()) {
            MainWindow::m_lumacache.insert(f, pix.scaled(50, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            QMetaObject::invokeMethod(this, "updateItemThumb", Q_ARG(QString, f));
        }
    }
}

void UrlListParamWidget::updateItemThumb(const QString &path)
{
    int ix = m_list->findData(path);
    if (ix > -1) {
        m_list->setItemIcon(ix, QPixmap::fromImage(MainWindow::m_lumacache.value(path)));
    }
}

bool UrlListParamWidget::isValidCubeFile(const QString &path)
{
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        int lineCt = 0;
        QTextStream in(&f);
        while (!in.atEnd() && lineCt < 30) {
            QString line = in.readLine();
            if (line.contains(QStringLiteral("LUT_3D_SIZE"))) {
                f.close();
                return true;
            }
        }
        f.close();
    }
    return false;
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
        if (m_isLutList && urlString.toLower().endsWith(QLatin1String(".cube"))) {
            if (isValidCubeFile(urlString)) {
                Q_EMIT valueChanged(m_index, urlString, true);
                slotRefresh();
                return;
            } else {
                pCore->displayMessage(i18n("Invalid LUT file %1", urlString), ErrorMessage);
            }
        } else {
            Q_EMIT valueChanged(m_index, urlString, true);
            slotRefresh();
            return;
        }
    }
    m_list->setCurrentIndex(m_currentIndex);
}
