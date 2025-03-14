/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transitionlistwidget.hpp"
#include "../model/transitiontreemodel.hpp"
#include "assets/assetlist/view/asseticonprovider.hpp"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "mainwindow.h"
#include "mltconnection.h"
#include "transitionicondelegate.hpp"
#include "transitions/transitionlist/model/transitionfilter.hpp"
#include "transitions/transitionsrepository.hpp"

#include <KMessageBox>
#include <QDir>
#include <QHeaderView>
#include <QProcess>
#include <QStandardPaths>
#include <QToolBar>

TransitionListWidget::TransitionListWidget(QWidget *parent)
    : AssetListWidget(false, parent)
{
    m_model = TransitionTreeModel::construct(true, this);
    m_proxyModel = std::make_unique<TransitionFilter>(this);
    m_proxyModel->setSourceModel(m_model.get());
    m_proxyModel->setSortRole(AssetTreeModel::NameRole);
    m_proxyModel->sort(0, Qt::AscendingOrder);

    // Set up tree view
    m_effectsTree->setModel(m_proxyModel.get());
    m_effectsTree->setColumnHidden(1, true);
    m_effectsTree->setColumnHidden(2, true);
    m_effectsTree->setColumnHidden(3, true);
    m_effectsTree->setColumnHidden(4, true);
    m_effectsTree->header()->setStretchLastSection(true);

    // Set up icon view with custom delegate
    m_effectsIcon->setModel(m_proxyModel.get());
    m_iconDelegate = new TransitionIconDelegate(this);

    // Set the preview directory
    QString previewDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/transitions/previews");
    // Ensure the directory exists
    QDir().mkpath(previewDir);
    qDebug() << "Setting transition preview directory to:" << previewDir;
    m_iconDelegate->setPreviewDirectory(previewDir);

    m_effectsIcon->setItemDelegate(m_iconDelegate);
    m_effectsIcon->setGridSize(QSize(200, 150));

    // Connect selection models
    QItemSelectionModel *sel = m_effectsTree->selectionModel();
    connect(sel, &QItemSelectionModel::currentChanged, this, &AssetListWidget::updateAssetInfo);

    // Add "Generate Previews" action to toolbar
    QAction *generateAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Generate Previews"), this);
    connect(generateAction, &QAction::triggered, this, &TransitionListWidget::generatePreviews);
    m_toolbar->addAction(generateAction);
}

TransitionListWidget::~TransitionListWidget() {}

bool TransitionListWidget::isAudio(const QString &assetId) const
{
    return TransitionsRepository::get()->isAudio(assetId);
}

QString TransitionListWidget::getMimeType(const QString &assetId) const
{
    Q_UNUSED(assetId);
    return QStringLiteral("kdenlive/composition");
}

void TransitionListWidget::setFilterType(const QString &type)
{
    if (type == "favorites") {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::Favorites);
    } else if (type == "transition") {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::VideoTransition);
    } else {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(false, AssetListType::AssetType::Favorites);
    }
}

void TransitionListWidget::refreshLumas()
{
    MltConnection::refreshLumas();
    // TODO: refresh currently displayed trans ?
}

void TransitionListWidget::reloadCustomEffectIx(const QModelIndex &) {}

void TransitionListWidget::reloadCustomEffect(const QString &) {}

void TransitionListWidget::reloadTemplates() {}

void TransitionListWidget::editCustomAsset(const QModelIndex &) {}

void TransitionListWidget::exportCustomEffect(const QModelIndex &) {}

void TransitionListWidget::generatePreviews()
{
    // script path is hardcoded for now
    QString scriptPath = QStringLiteral("/home/mr-swastik/kde/src/kdenlive/scripts/generate_transition_previews.py");

    // If not found, try standard locations
    if (!QFile::exists(scriptPath)) {
        scriptPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/generate_transition_previews.py"));

        // If still not found, try relative to application directory
        if (scriptPath.isEmpty()) {
            scriptPath = QCoreApplication::applicationDirPath() + QStringLiteral("/../scripts/generate_transition_previews.py");
            if (!QFile::exists(scriptPath)) {
                scriptPath = QCoreApplication::applicationDirPath() + QStringLiteral("/scripts/generate_transition_previews.py");
            }
        }
    }

    qDebug() << "Using script path:" << scriptPath;

    // If still not found, show error
    if (!QFile::exists(scriptPath)) {
        KMessageBox::error(this, i18n("Could not find the preview generation script. Please make sure it is installed correctly."));
        return;
    }

    // Create output directory
    QString outputDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/transitions/previews");
    QDir().mkpath(outputDir);

    qDebug() << "Generating previews to:" << outputDir;

    // Run the script
    QProcess *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [process]() { qDebug() << "Preview generation:" << process->readAllStandardOutput(); });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process, outputDir](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    // Update the delegate's preview directory
                    m_iconDelegate->setPreviewDirectory(outputDir);

                    // Force refresh of the view
                    if (isIconView()) {
                        m_effectsIcon->reset();
                    }

                    KMessageBox::information(this, i18n("Transition previews have been generated successfully."));
                } else {
                    KMessageBox::error(this, i18n("Failed to generate transition previews. Check the application log for details."));
                }

                process->deleteLater();
            });

    // Start the process
    QStringList args;
    args << QStringLiteral("--output-dir") << outputDir;
    args << QStringLiteral("--width") << QStringLiteral("320");
    args << QStringLiteral("--height") << QStringLiteral("180");

    process->start(QStringLiteral("python3"), QStringList() << scriptPath << args);

    KMessageBox::information(this, i18n("Generating transition previews. This may take a few minutes..."));
}
