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

#include <QDir>
#include <QHeaderView>
#include <QProcess>
#include <QStandardPaths>
#include <QToolBar>

TransitionListWidget::TransitionListWidget(QAction *includeList, QAction *tenBit, QWidget *parent)
    : AssetListWidget(false, includeList, tenBit, parent)
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
    m_effectsTree->setColumnHidden(5, true);
    m_effectsTree->setColumnHidden(6, true);
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
    // TODO: icon size relative to project profile
    m_effectsIcon->setIconSize(QSize(120, 68));
    m_effectsIcon->setViewMode(QListView::IconMode);
    m_effectsIcon->setItemDelegate(m_iconDelegate);
    m_effectsIcon->setMouseTracking(true);

    // Connect selection models
    QItemSelectionModel *sel = m_effectsTree->selectionModel();
    connect(sel, &QItemSelectionModel::currentChanged, this, &TransitionListWidget::updateAssetInfo);

    QItemSelectionModel *iconSel = m_effectsIcon->selectionModel();
    connect(iconSel, &QItemSelectionModel::currentChanged, this, &TransitionListWidget::updateTransitionInfo);
    connect(m_effectsIcon, &QAbstractItemView::entered, this, &TransitionListWidget::iconViewEntered);

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
    } else if (type == "lumas") {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::LumaTransition);
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
    // Find the script in the standard install location
    QString scriptPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/generate_transition_previews.py"));

    // If not found, try fallback locations
    if (scriptPath.isEmpty()) {
        // Try development environment location
        QString devPath = QString::fromLocal8Bit(qgetenv("KDENLIVE_SOURCE_DIR"));
        if (!devPath.isEmpty()) {
            scriptPath = devPath + QStringLiteral("/data/scripts/generate_transition_previews.py");
            if (!QFile::exists(scriptPath)) {
                scriptPath.clear();
            }
        }

        // If still not found, try relative to application directory
        if (scriptPath.isEmpty()) {
            scriptPath = QCoreApplication::applicationDirPath() + QStringLiteral("/../share/kdenlive/scripts/generate_transition_previews.py");
            if (!QFile::exists(scriptPath)) {
                scriptPath = QCoreApplication::applicationDirPath() + QStringLiteral("/scripts/generate_transition_previews.py");
            }
        }
    }

    qDebug() << "Using script path:" << scriptPath;

    // If still not found, show error
    if (scriptPath.isEmpty() || !QFile::exists(scriptPath)) {
        m_infoBar->setMessageType(KMessageWidget::Warning);
        m_infoBar->setText(i18n("Could not find the preview generation script. Please make sure it is installed correctly."));
        m_infoBar->setVisible(true);
        QTimer::singleShot(7000, this, [&]() { m_infoBar->setVisible(false); });
        return;
    }

    // Find the transition parameters file
    QString paramFile = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("transitions/parameters.txt"));

    // If not found, try fallback locations
    if (paramFile.isEmpty()) {
        // Try development environment location
        QString devPath = QString::fromLocal8Bit(qgetenv("KDENLIVE_SOURCE_DIR"));
        if (!devPath.isEmpty()) {
            paramFile = devPath + QStringLiteral("/data/transitions/parameters.txt");
            if (!QFile::exists(paramFile)) {
                paramFile.clear();
            }
        }

        // Try relative to app directory
        if (paramFile.isEmpty()) {
            paramFile = QCoreApplication::applicationDirPath() + QStringLiteral("/../share/kdenlive/transitions/parameters.txt");
            if (!QFile::exists(paramFile)) {
                paramFile = QCoreApplication::applicationDirPath() + QStringLiteral("/transitions/parameters.txt");
            }
        }
    }

    qDebug() << "Using parameters file:" << (paramFile.isEmpty() ? "not found" : paramFile);

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
                    m_infoBar->setMessageType(KMessageWidget::Information);
                    m_infoBar->setText(i18n("Transition previews have been generated successfully."));
                    m_infoBar->setVisible(true);
                    QTimer::singleShot(5000, this, [&]() { m_infoBar->setVisible(false); });
                } else {
                    m_infoBar->setMessageType(KMessageWidget::Warning);
                    m_infoBar->setText(i18n("Failed to generate transition previews. Check the application log for details."));
                    m_infoBar->setVisible(true);
                    QTimer::singleShot(7000, this, [&]() { m_infoBar->setVisible(false); });
                }

                process->deleteLater();
            });

    // Start the process
    QStringList args;
    args << QStringLiteral("--output-dir") << outputDir;
    args << QStringLiteral("--width") << QStringLiteral("320");
    args << QStringLiteral("--height") << QStringLiteral("180");

    // Add parameter file if found
    if (!paramFile.isEmpty()) {
        args << QStringLiteral("--param-file") << paramFile;
    }
    const QStringList lumaFolders = QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("lumas"), QStandardPaths::LocateDirectory);
    if (!lumaFolders.isEmpty()) {
        args << QStringLiteral("--luma-path") << lumaFolders.join(QLatin1Char(' '));
    }

#ifdef Q_OS_WIN
    const QString pythonName = QStringLiteral("python");
#else
    const QString pythonName = QStringLiteral("python3");
#endif
    const QString pythonExe = QStandardPaths::findExecutable(pythonName);
    if (!pythonExe.isEmpty()) {
        args.prepend(scriptPath);
        process->start(pythonExe, args);
    } else {
        m_infoBar->setMessageType(KMessageWidget::Warning);
        m_infoBar->setText(i18n("Python interpreter not found. Please make sure Python is installed."));
        m_infoBar->setVisible(true);
        QTimer::singleShot(7000, this, [&]() { m_infoBar->setVisible(false); });
        process->deleteLater();
        return;
    }

    m_infoBar->setMessageType(KMessageWidget::Information);
    m_infoBar->setText(i18n("Generating transition previews. This may take a few minutes..."));
    m_infoBar->setVisible(true);
    QTimer::singleShot(5000, this, [&]() { m_infoBar->setVisible(false); });
}

void TransitionListWidget::switchTenBitFilter()
{
    KdenliveSettings::setTransitionsFilter(m_filterButton->isChecked());
    m_proxyModel->invalidate();
}

void TransitionListWidget::showLumas()
{
    KdenliveSettings::setTransitionsFilter(m_filterButton->isChecked());
    m_proxyModel->invalidate();
}

void TransitionListWidget::updateTransitionInfo(const QModelIndex &current, const QModelIndex &previous)
{
    if (previous.isValid()) {
        // Stop previous animation
        QString previousId = previous.data(AssetTreeModel::IdRole).toString();
        if (previousId.isEmpty() || previousId == QLatin1String("root")) {
            // Nothing to do
        } else {
            auto type = previous.data(AssetTreeModel::TypeRole).value<AssetListType::AssetType>();
            if (type == AssetListType::AssetType::LumaTransition) {
                previousId = QFileInfo(previousId).baseName();
            }
            auto movie = m_iconDelegate->getMovie(previousId);
            if (movie) {
                movie->stop();
            }
        }
    }
    // Get transition ID
    QString transitionId = current.data(AssetTreeModel::IdRole).toString();
    if (transitionId.isEmpty() || transitionId == QLatin1String("root")) {
        return;
    }
    // Luma transition id is the full path to luma, adjust
    auto type = current.data(AssetTreeModel::TypeRole).value<AssetListType::AssetType>();
    if (type == AssetListType::AssetType::LumaTransition) {
        transitionId = QFileInfo(transitionId).baseName();
    }
    auto movie = m_iconDelegate->getMovie(transitionId);
    if (movie) {
        // Connect to frameChanged to trigger repaint
        QObject::disconnect(m_animationConnection);
        m_animationConnection = connect(movie, &QMovie::frameChanged, [this, current]() {
            // Find all list views using this delegate
            m_effectsIcon->update(current);
        });
        movie->start();
    }
    updateAssetInfo(current, previous);
}

void TransitionListWidget::iconViewEntered(const QModelIndex &ix)
{
    if (!m_hoveredTransition.isEmpty()) {
        auto movie = m_iconDelegate->getMovie(m_hoveredTransition);
        if (movie) {
            movie->stop();
        }
        QObject::disconnect(m_hoverAnimationConnection);
        m_hoveredTransition.clear();
    }
    QString transitionId = ix.data(AssetTreeModel::IdRole).toString();
    if (transitionId.isEmpty() || transitionId == QLatin1String("root")) {
        return;
    }
    // Luma transition id is the full path to luma, adjust
    auto type = ix.data(AssetTreeModel::TypeRole).value<AssetListType::AssetType>();
    if (type == AssetListType::AssetType::LumaTransition) {
        transitionId = QFileInfo(transitionId).baseName();
    }
    auto movie = m_iconDelegate->getMovie(transitionId);
    if (movie) {
        m_hoveredTransition = transitionId;
        // Connect to frameChanged to trigger repaint
        QObject::disconnect(m_animationConnection);
        m_hoverAnimationConnection = connect(movie, &QMovie::frameChanged, [this, ix]() {
            // Find all list views using this delegate
            m_effectsIcon->update(ix);
        });
        movie->start();
        qDebug() << "Start Animation...";
    }
}
