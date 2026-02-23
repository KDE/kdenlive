/*
    SPDX-FileCopyrightText: 2021 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "urllistparamwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "mltconnection.h"
#include "timeline2/model/timelineitemmodel.hpp"

#include <QDirIterator>
#include <QFileDialog>
#include <QtConcurrent/QtConcurrentRun>

UrlListParamWidget::UrlListParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_listType(OTHERLIST)
{
    setupUi(this);

    // Get data from model
    const QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    const QString configFile = m_model->data(m_index, AssetParameterModel::NewStuffRole).toString();

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

    QStringList values = m_model->data(m_index, AssetParameterModel::ListValuesRole).toStringList();
    if (!values.isEmpty()) {
        if (values.first() == QLatin1String("%lumaPaths")) {
            m_listType = LUMALIST;
        } else if (values.first() == QLatin1String("%lutPaths")) {
            m_listType = LUTLIST;
        } else if (values.first() == QLatin1String("%maskPaths")) {
            m_listType = MASKLIST;
        }
    }
    if (m_listType == MASKLIST) {
        m_maskButton->setIcon(QIcon::fromTheme(QStringLiteral("path-mask-edit")));
        m_maskButton->setToolTip(i18n("Create or edit a mask for this clip"));
        connect(m_maskButton, &QToolButton::clicked, this, [this]() { Q_EMIT pCore->switchMaskPanel(true); });
    } else {
        m_maskButton->hide();
    }
    UrlListParamWidget::slotRefresh();

    // Q_EMIT the signal of the base class when appropriate
    // The connection is ugly because the signal "currentIndexChanged" is overloaded in QComboBox
    connect(this->m_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this](int index) {
        if (m_list->currentData() == QStringLiteral("custom_file")) {
            openFile();
        } else {
            m_currentIndex = index;
            if (m_listType == MASKLIST) {
                int in = m_list->currentData(Qt::UserRole + 1).toInt();
                int out = m_list->currentData(Qt::UserRole + 2).toInt();
                if (out == 0) {
                    out = -1;
                }
                /*const QModelIndex ix = m_model->getParamIndexFromName(QStringLiteral("in"));
                //Q_EMIT valueChanged(ix, QString::number(in), true);
                const QModelIndex ix2 = m_model->getParamIndexFromName(QStringLiteral("out"));
                //Q_EMIT valueChanged(ix2, QString::number(out), true);
                const QList<QModelIndex> ixes{ix, ix2, m_index};
                const QStringList sourceValues{QString::number(in), QString::number(out), m_list->currentData().toString()};
                Q_EMIT valuesChanged(ixes, sourceValues, {}, true);*/
                paramVector paramValues;
                paramValues.append({m_model->data(m_index, AssetParameterModel::NameRole).toString(), m_list->currentData()});
                paramValues.append({QStringLiteral("in"), in});
                paramValues.append({QStringLiteral("out"), out});
                QMetaObject::invokeMethod(m_model.get(), "setParametersFromTask", Q_ARG(paramVector, paramValues));
                // m_model->setParametersFromTask(paramValues);
            } else {
                Q_EMIT valueChanged(m_index, m_list->currentData().toString(), true);
            }
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
    QMap<QString, QString> listValues;
    QStringList thumbnailsToBuild;
    filter.remove(0, filter.indexOf("(") + 1);
    filter.remove(filter.indexOf(")") - 1, -1);
    m_fileExt = filter.split(" ");
    if (!values.isEmpty() && values.first() == QLatin1String("%lumaPaths")) {
        // special case: Luma files
        m_listType = LUMALIST;
        values.clear();
        names.clear();
        values = pCore->getLumasForProfile();
        // Fetch names
        for (auto &v : values) {
            QString lumaName = pCore->nameForLumaFile(QFileInfo(v).fileName());
            if (listValues.contains(lumaName)) {
                // Duplicate name, add a suffix
                const QString baseName = lumaName;
                int i = 2;
                lumaName = baseName + QStringLiteral(" / %1").arg(i);
                while (listValues.contains(lumaName)) {
                    i++;
                    lumaName = baseName + QStringLiteral(" / %1").arg(i);
                }
            }
            listValues.insert(lumaName, v);
        }
        values.clear();
        m_list->addItem(i18n("None (Dissolve)"));
        bool builtIn = false;
        QFileInfo info(currentValue);
        // This is an MLT build luma
        static const QRegularExpression lumaRegexp("^luma[0-2][0-9].pgm$");
        if (lumaRegexp.match(info.fileName()).hasMatch() && !info.exists()) {
            // This is a built in luma.
            currentValue = info.fileName();
            builtIn = true;
        }
        // add all matching files in the location of the current item too
        if (!currentValue.isEmpty() && !builtIn) {
            addItemsInSameFolder(currentValue, &listValues);
        }
        QMapIterator<QString, QString> i(listValues);
        while (i.hasNext()) {
            i.next();
            const QString &entry = i.value();
            m_list->addItem(i.key(), entry);
            int ix = m_list->findData(entry);
            // Create thumbnails
            if (!entry.isEmpty() &&
                (entry.endsWith(QLatin1String(".png"), Qt::CaseInsensitive) || entry.endsWith(QLatin1String(".pgm"), Qt::CaseInsensitive))) {
                if (MainWindow::m_lumacache.contains(entry)) {
                    m_list->setItemIcon(ix, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
                } else {
                    // render thumbnails in another thread, except for build-ins
                    if (!lumaRegexp.match(entry).hasMatch()) {
                        thumbnailsToBuild << entry;
                    }
                }
            }
        }
    } else if (!values.isEmpty() && values.first() == QLatin1String("%lutPaths")) {
        // special case: LUT files
        values.clear();
        names.clear();
        m_listType = LUTLIST;
        // check for Kdenlive installed luts files
        QStringList customLuts = QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("luts"), QStandardPaths::LocateDirectory);
        const QString lastUsedPath = KRecentDirs::dir(QStringLiteral(":KdenliveUrlLutParamFolder"));
        if (!lastUsedPath.isEmpty()) {
            customLuts << lastUsedPath;
        }
        customLuts.removeDuplicates();
        for (const QString &folderpath : std::as_const(customLuts)) {
            QDir dir(folderpath);
            if (!dir.exists()) {
                continue;
            }
            QDirIterator it(dir.absolutePath(), m_fileExt, QDir::Files,
                            folderpath == lastUsedPath ? QDirIterator::NoIteratorFlags : QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                listValues.insert(QFileInfo(path).baseName(), path);
            }
        }
        // add all matching files in the location of the current item too
        if (!currentValue.isEmpty()) {
            addItemsInSameFolder(currentValue, &listValues);
        }
        // Ensure the lut files are valid
        QMapIterator<QString, QString> l(listValues);
        QStringList lutsToRemove;
        while (l.hasNext()) {
            l.next();
            if (l.value().endsWith(QLatin1String(".cube"), Qt::CaseInsensitive) && !KdenliveSettings::validated_luts().contains(l.value())) {
                // Open LUT file and check validity
                if (isValidCubeFile(l.value())) {
                    QStringList validated = KdenliveSettings::validated_luts();
                    validated << l.value();
                    KdenliveSettings::setValidated_luts(validated);
                } else {
                    qDebug() << ":::: FOUND INVALID LUT FILE: " << l.value();
                    lutsToRemove << l.key();
                }
            }
        }
        while (!lutsToRemove.isEmpty()) {
            const QString lut = lutsToRemove.takeFirst();
            listValues.remove(lut);
        }
        QMapIterator<QString, QString> i(listValues);
        while (i.hasNext()) {
            i.next();
            const QString &entry = i.value();
            m_list->addItem(i.key(), entry);
        }
    } else if (!values.isEmpty() && values.first() == QLatin1String("%maskPaths")) {
        // check for Kdenlive project masks
        values.clear();
        names.clear();
        m_listType = MASKLIST;
        QString binId;
        QVector<MaskInfo> masks;
        ObjectId owner = m_model->getOwnerId();
        if (owner.type == KdenliveObjectType::TimelineClip) {
            auto tl = pCore->currentDoc()->getTimeline(owner.uuid);
            if (tl) {
                binId = tl->getClipBinId(owner.itemId);
            }
        } else if (owner.type == KdenliveObjectType::BinClip) {
            binId = QString::number(owner.itemId);
        }
        if (!binId.isEmpty()) {
            masks = pCore->projectItemModel()->getClipMasks(binId);
        }
        // Check that current value is in the list or add a new entry
        bool currentFound = currentValue.isEmpty();
        for (const auto &mask : std::as_const(masks)) {
            if (!mask.isValid) {
                continue;
            }
            const QString filePath = mask.maskFile;
            if (!currentFound && filePath == currentValue) {
                currentFound = true;
            }
            m_list->addItem(mask.maskName, filePath);
            int ix = m_list->findData(filePath);
            m_list->setItemData(ix, mask.in, Qt::UserRole + 1);
            m_list->setItemData(ix, mask.out, Qt::UserRole + 2);
            // Create thumbnails
            QString thumbFile = filePath.section(QLatin1Char('.'), 0, -2);
            thumbFile.append(QStringLiteral(".png"));
            m_list->setItemIcon(ix, QPixmap::fromImage(QImage(thumbFile)));
        }
        if (!currentFound) {
            QFileInfo info(currentValue);
            m_list->addItem(info.baseName(), currentValue);
        }
    }

    if (m_listType == OTHERLIST) {
        // build ui list
        QMap<QString, QString> entryMap;
        int ix = 0;
        // Put all name/value combinations in a map
        for (const QString &value : std::as_const(values)) {
            if (ix < names.count()) {
                entryMap.insert(names.at(ix), value);
            }
            ix++;
        }
    } else {
        m_list->addItem(i18n("Custom…"), QStringLiteral("custom_file"));
    }

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
        m_thumbJob = QtConcurrent::run(&UrlListParamWidget::buildThumbnails, this, thumbnailsToBuild);
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

void UrlListParamWidget::addItemsInSameFolder(const QString currentValue, QMap<QString, QString> *listValues)
{
    QDir dir = QFileInfo(currentValue).absoluteDir();
    if (dir.exists()) {
        QStringList entries = dir.entryList(m_fileExt, QDir::Files);
        for (const auto &filename : std::as_const(entries)) {
            const QString path = dir.absoluteFilePath(filename);
            if (std::find((*listValues).cbegin(), (*listValues).cend(), path) == (*listValues).cend()) {
                (*listValues).insert(QFileInfo(filename).baseName(), path);
            }
        }
        // make sure the current value is added. If it is a duplicate we remove it later
        if (std::find((*listValues).cbegin(), (*listValues).cend(), currentValue) == (*listValues).cend()) {
            if (QFileInfo::exists(currentValue)) {
                (*listValues).insert(QFileInfo(currentValue).baseName(), currentValue);
            }
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
            const QString line = in.readLine();
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
    QString path;
    switch (m_listType) {
    case LUTLIST:
        path = KRecentDirs::dir(QStringLiteral(":KdenliveUrlLutParamFolder"));
        break;
    case MASKLIST: {
        bool ok;
        QDir dir = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
        if (ok) {
            path = dir.absolutePath();
        }
        break;
    }
    case LUMALIST:
    default:
        path = KRecentDirs::dir(QStringLiteral(":KdenliveUrlListParamFolder"));
    }
    const QString filter = m_model->data(m_index, AssetParameterModel::FilterRole).toString();

    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }

    const QString urlString = QFileDialog::getOpenFileName(this, QString(), path, filter);

    if (!urlString.isEmpty()) {
        if (m_listType == LUTLIST) {
            KRecentDirs::add(QStringLiteral(":KdenliveUrlLutParamFolder"), QFileInfo(urlString).absolutePath());
        } else if (m_listType == LUMALIST || m_listType == OTHERLIST) {
            KRecentDirs::add(QStringLiteral(":KdenliveUrlListParamFolder"), QFileInfo(urlString).absolutePath());
        }
        if (m_listType == LUTLIST && urlString.endsWith(QLatin1String(".cube"), Qt::CaseInsensitive)) {
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
