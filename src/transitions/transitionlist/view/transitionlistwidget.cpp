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
#include <QScrollBar>
#include <QStandardPaths>
#include <QToolBar>
#include <qsplitter.h>
#include <qstackedwidget.h>

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
    int iconHeight = QFontMetrics(font()).lineSpacing() * 4.5;
    QSize iconSize;
    if (pCore->getCurrentDar() > 1.) {
        iconSize = QSize(iconHeight, iconHeight / pCore->getCurrentDar());
    } else {
        iconSize = QSize(iconHeight / pCore->getCurrentDar(), iconHeight);
    }
    m_effectsIcon->setIconSize(iconSize);
    m_iconDelegate = new TransitionIconDelegate(iconSize, this);

    m_effectsIcon->setViewMode(QListView::IconMode);
    m_effectsIcon->setItemDelegate(m_iconDelegate);
    m_effectsIcon->setMouseTracking(true);

    // Connect selection models
    QItemSelectionModel *sel = m_effectsTree->selectionModel();
    connect(sel, &QItemSelectionModel::currentChanged, this, &TransitionListWidget::updateAssetInfo);

    QItemSelectionModel *iconSel = m_effectsIcon->selectionModel();
    connect(iconSel, &QItemSelectionModel::currentChanged, this, &TransitionListWidget::updateTransitionInfo);
    connect(m_effectsIcon, &QAbstractItemView::entered, this, &TransitionListWidget::iconViewEntered);
    connect(m_effectsIcon, &AssetListView::exited, this, &TransitionListWidget::iconViewExited);

    // Add "Generate Previews" action to toolbar, only if python is found
#ifdef Q_OS_WIN
    const QString pythonName = QStringLiteral("python");
#else
    const QString pythonName = QStringLiteral("python3");
#endif
    const QString pythonExe = QStandardPaths::findExecutable(pythonName);
    if (!pythonExe.isEmpty()) {
        m_generatePreviewAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Generate Previews"), this);
        connect(m_generatePreviewAction, &QAction::triggered, this, &TransitionListWidget::generatePreviews);
        m_toolbar->addAction(m_generatePreviewAction);
    }
    connect(this, &AssetListWidget::checkAssetPreview, this, &TransitionListWidget::checkPreviews);
    connect(m_model.get(), &AssetTreeModel::rowsInserted, this, [this]() { m_proxyModel->sort(0, Qt::AscendingOrder); });

    if (!KdenliveSettings::showTransitionsInfo()) {
        m_viewSplitter->setSizes({50, 0});
    } else {
        const QByteArray restoreData = KdenliveSettings::transitionsInfoHeight().toLatin1();
        if (restoreData.isEmpty()) {
            m_viewSplitter->setSizes({50, 20});
            const QByteArray splitterData = m_viewSplitter->saveState();
            KdenliveSettings::setTransitionsInfoHeight(QString::fromLatin1(splitterData));
        } else {
            // Use a single-shot timer to restore the state
            QTimer::singleShot(0, this, [this, restoreData]() { m_viewSplitter->restoreState(restoreData); });
        }
    }
    connect(m_viewSplitter, &QSplitter::splitterMoved, this, [this]() {
        const QByteArray splitterData = m_viewSplitter->saveState();
        KdenliveSettings::setTransitionsInfoHeight(QString::fromLatin1(splitterData));
    });
    connect(m_effectsIcon->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        auto currentIndex = m_effectsIcon->indexAt(m_effectsIcon->mapFromGlobal(QCursor::pos()));
        if (currentIndex.isValid()) {
            iconViewEntered(currentIndex);
        }
    });
}

TransitionListWidget::~TransitionListWidget()
{
    if (m_previewProcess) {
        disconnect(m_previewProcess.get(), nullptr, nullptr, nullptr);
        m_previewProcess->kill();
        m_previewProcess->waitForFinished();
    }
}

void TransitionListWidget::switchSplitter(bool enable)
{
    KdenliveSettings::setShowTransitionsInfo(enable);
    if (enable) {
        const QByteArray restoreData = KdenliveSettings::transitionsInfoHeight().toLatin1();
        if (restoreData.isEmpty()) {
            m_viewSplitter->setSizes({50, 20});
            const QByteArray saveData = m_viewSplitter->saveState();
            KdenliveSettings::setTransitionsInfoHeight(QString::fromLatin1(saveData));
        } else {
            m_viewSplitter->restoreState(restoreData);
        }
    } else {
        m_viewSplitter->setSizes({50, 0});
    }
}

void TransitionListWidget::checkPreviews(bool force)
{
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/transitions/previews");
    QDir previewFolder(outputDir);
    if (m_generatePreviewAction && (force || !previewFolder.exists() || previewFolder.isEmpty())) {
        generatePreviews();
    }
}

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
    if (m_previewProcess) {
        // Already running, abort
        return;
    }
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

    // Create output directory
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/transitions/previews");
    QDir().mkpath(outputDir);

    qDebug() << "Generating previews to:" << outputDir;

    // Run the script
    m_previewProcess.reset(new QProcess(this));
    m_previewProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_previewProcess.get(), &QProcess::readyReadStandardOutput, this,
            [this]() { qDebug() << "Preview generation:" << m_previewProcess->readAllStandardOutput(); });

    connect(m_previewProcess.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &TransitionListWidget::previewDone,
            Qt::UniqueConnection);

    const QStringList xmlFolders = TransitionsRepository::get()->assetDirs();
    QStringList encodedXmlFolders;
    for (auto &l : xmlFolders) {
        encodedXmlFolders << QUrl::toPercentEncoding(l, "{/}");
    }
    // Start the process
    QStringList args;
    // Don't generate previews for default transitions
    // args << QStringLiteral("--xml-dir") << encodedXmlFolders.join(QLatin1Char(' '));
    args << QStringLiteral("--output-dir") << outputDir;
    args << QStringLiteral("--width") << QStringLiteral("176");
    args << QStringLiteral("--height") << QStringLiteral("99");
    args << QStringLiteral("--file-format") << QStringLiteral("gif");

    const QStringList lumaFolders = QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("lumas"), QStandardPaths::LocateDirectory);
    QStringList encodedLumas;
    for (auto &l : lumaFolders) {
        encodedLumas << QUrl::toPercentEncoding(l, "{/}");
    }
    if (!lumaFolders.isEmpty()) {
        args << QStringLiteral("--luma-path") << encodedLumas.join(QLatin1Char(' '));
    }

#ifdef Q_OS_WIN
    const QString pythonName = QStringLiteral("python");
#else
    const QString pythonName = QStringLiteral("python3");
#endif
    const QString pythonExe = QStandardPaths::findExecutable(pythonName);
    if (!pythonExe.isEmpty()) {
        args.prepend(scriptPath);
        m_generatePreviewAction->setEnabled(false);
        m_previewProcess->start(pythonExe, args);
    } else {
        m_infoBar->setMessageType(KMessageWidget::Warning);
        m_infoBar->setText(i18n("Python interpreter not found. Please make sure Python is installed."));
        m_infoBar->setVisible(true);
        QTimer::singleShot(7000, this, [&]() { m_infoBar->setVisible(false); });
        m_previewProcess.reset();
        return;
    }

    m_infoBar->setMessageType(KMessageWidget::Information);
    m_infoBar->setText(i18n("Generating transition previews. This may take a few minutes..."));
    m_infoBar->setVisible(true);
}

void TransitionListWidget::previewDone(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_generatePreviewAction->setEnabled(true);
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
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
    m_previewProcess.reset();
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
    // Get transition ID
    QString transitionId = current.data(AssetTreeModel::IdRole).toString();
    if (transitionId.isEmpty() || transitionId == QLatin1String("root")) {
        return;
    }
    updateAssetInfo(current, previous);
}

void TransitionListWidget::iconViewEntered(const QModelIndex &ix)
{
    QString transitionId = ix.data(AssetTreeModel::IdRole).toString();
    if (transitionId.isEmpty() || transitionId == QLatin1String("root")) {
        iconViewExited();
        return;
    }
    // Luma transition id is the full path to luma, adjust
    auto type = ix.data(AssetTreeModel::TypeRole).value<AssetListType::AssetType>();
    if (type == AssetListType::AssetType::LumaTransition) {
        transitionId = QFileInfo(transitionId).baseName();
    }
    if (transitionId == m_hoveredTransition) {
        return;
    }
    iconViewExited();
    auto movie = m_iconDelegate->getMovie(transitionId, true);
    if (movie) {
        m_hoveredTransition = transitionId;
        // Connect to frameChanged to trigger repaint
        m_hoverAnimationConnection = connect(movie, &QMovie::updated, [this, ix]() {
            auto index = m_proxyModel->mapToSource(ix);
            Q_EMIT m_model->dataChanged(index, index, {Qt::DecorationRole, Qt::DisplayRole});
        });
        movie->start();
        qDebug() << "Start Animation...";
    }
}

void TransitionListWidget::iconViewExited()
{
    if (!m_hoveredTransition.isEmpty()) {
        auto movie = m_iconDelegate->getMovie(m_hoveredTransition, true);
        movie->stop();
        QObject::disconnect(m_hoverAnimationConnection);
        m_hoveredTransition.clear();
    }
}
