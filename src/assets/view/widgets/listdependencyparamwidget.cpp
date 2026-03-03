/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "listdependencyparamwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "mainwindow.h"

#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QStandardPaths>

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>

ListDependencyParamWidget::ListDependencyParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);
    m_infoMessage->hide();
    connect(m_infoMessage, &KMessageWidget::linkActivated, this, [this](const QString &contents) {
        const QUrl linkUrl(contents);
        if (linkUrl.isLocalFile()) {
            // Ensure folder or file exists
            QFileInfo info(linkUrl.toLocalFile());
            if (!info.exists()) {
                QDir paramFolder(linkUrl.toLocalFile());
                paramFolder.mkpath(QStringLiteral("."));
            }
        }
        auto *job = new KIO::OpenUrlJob(linkUrl);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        // methods like setRunExecutables, setSuggestedFilename, setEnableExternalBrowser, setFollowRedirections
        // exist in both classes
        job->start();
    });
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_list->setIconSize(QSize(50, 30));
    setMinimumHeight(m_list->sizeHint().height());

    QString dependencies = m_model->data(m_index, AssetParameterModel::ListDependenciesRole).toString();
    if (!dependencies.isEmpty()) {
        // We have conditional dependencies, some values in the list might not be available.
        QDomDocument doc;
        doc.setContent(dependencies);
        QDomNodeList deps = doc.elementsByTagName(QLatin1String("paramdependencies"));
        for (int i = 0; i < deps.count(); i++) {
            const QString modelName = deps.at(i).toElement().attribute(QLatin1String("value"));
            QString infoText = deps.at(i).toElement().text();
            const QString folder = deps.at(i).toElement().attribute(QLatin1String("folder"));
            if (!folder.isEmpty()) {
                m_dependencyFiles.insert(modelName, {folder, deps.at(i).toElement().attribute(QLatin1String("files")).split(QLatin1Char(';'))});
                QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + folder);
                infoText.replace(QLatin1String("%folder"), dir.absolutePath());
            }
            m_dependencyInfos.insert(modelName, infoText);
        }
    }

    slotRefresh();

    // Q_EMIT the signal of the base class when appropriate
    connect(this->m_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString val = m_list->itemData(m_list->currentIndex()).toString();
        Q_EMIT valueChanged(m_index, val, true);
        checkDependencies(val);
    });
}

void ListDependencyParamWidget::setCurrentIndex(int index)
{
    m_list->setCurrentIndex(index);
}

void ListDependencyParamWidget::setCurrentText(const QString &text)
{
    m_list->setCurrentText(text);
}

void ListDependencyParamWidget::addItem(const QString &text, const QVariant &value)
{
    m_list->addItem(text, value);
}

void ListDependencyParamWidget::setItemIcon(int index, const QIcon &icon)
{
    m_list->setItemIcon(index, icon);
}

void ListDependencyParamWidget::setIconSize(const QSize &size)
{
    m_list->setIconSize(size);
}

void ListDependencyParamWidget::slotShowComment(bool /*show*/) {}

QString ListDependencyParamWidget::getValue()
{
    return m_list->currentData().toString();
}

void ListDependencyParamWidget::checkDependencies(const QString &val)
{
    bool missingDep = false;
    if (m_dependencyInfos.contains(val)) {
        // Check dependency
        if (m_dependencyFiles.contains(val)) {
            QPair<QString, QStringList> fileData = m_dependencyFiles.value(val);
            QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + fileData.first);
            if (fileData.first == QLatin1String("/opencvmodels")) {
                m_model->setParameter(QStringLiteral("modelsfolder"), dir.absolutePath(), false);
            }
            for (const QString &file : std::as_const(fileData.second)) {
                if (!dir.exists(file)) {
                    m_infoMessage->setText(m_dependencyInfos.value(val));
                    m_infoMessage->animatedShow();
                    setMinimumHeight(m_list->sizeHint().height() + m_infoMessage->sizeHint().height());
                    Q_EMIT updateHeight();
                    missingDep = true;
                    break;
                }
            }
        }
    }
    if (!missingDep) {
        m_infoMessage->hide();
        setMinimumHeight(m_list->sizeHint().height());
        Q_EMIT updateHeight();
    }
}

void ListDependencyParamWidget::slotRefresh()
{
    const QSignalBlocker bk(m_list);
    m_list->clear();
    QStringList names = m_model->data(m_index, AssetParameterModel::ListNamesRole).toStringList();
    QStringList values = m_model->data(m_index, AssetParameterModel::ListValuesRole).toStringList();
    QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    if (value != m_lastProcessedAlgo) {
        // Ensure dependencies are met
        checkDependencies(value);
    }
    m_lastProcessedAlgo = value;
    if (values.first() == QLatin1String("%lumaPaths")) {
        // Special case: Luma files
        // Create thumbnails
        values = pCore->getLumasForProfile();
        m_list->addItem(i18n("None (Dissolve)"));
        for (int j = 0; j < values.count(); ++j) {
            const QString &entry = values.at(j);
            const QString name = values.at(j).section(QLatin1Char('/'), -1);
            m_list->addItem(pCore->nameForLumaFile(name), entry);
            if (!entry.isEmpty() && (entry.endsWith(QLatin1String(".png")) || entry.endsWith(QLatin1String(".pgm")))) {
                if (MainWindow::m_lumacache.contains(entry)) {
                    m_list->setItemIcon(j + 1, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
                }
            }
        }
        if (!value.isEmpty() && values.contains(value)) {
            m_list->setCurrentIndex(values.indexOf(value) + 1);
        }
    } else {
        if (names.count() != values.count()) {
            names = values;
        }
        for (int i = 0; i < names.count(); i++) {
            m_list->addItem(names.at(i), values.at(i));
        }
        if (!value.isEmpty()) {
            int ix = m_list->findData(value);
            if (ix > -1) {
                m_list->setCurrentIndex(ix);
            }
        }
    }
}
