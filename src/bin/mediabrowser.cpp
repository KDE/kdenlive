/*
SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mediabrowser.h"
#include "bin.h"
#include "clipcreator.hpp"
#include "core.h"
#include "dialogs/clipcreationdialog.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "project/dialogs/slideshowclip.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KDirLister>
#include <KFileFilterCombo>
#include <KFilePlacesModel>
#include <KFilePreviewGenerator>
#include <KIconLoader>
#include <KLocalizedString>
#include <KRecentDirs>
#include <KSharedConfig>
#include <KUrlComboBox>
#include <KUrlCompletion>
#include <KUrlNavigator>
#include <QAction>
#include <QCheckBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMimeDatabase>
#include <QProcessEnvironment>
#include <QToolBar>
#include <QVBoxLayout>
#include <kio_version.h>

MediaBrowser::MediaBrowser(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setSpacing(0);
    QToolBar *tb = new QToolBar(this);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);
    tb->setIconSize(iconSize);
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    KFilePlacesModel *places = new KFilePlacesModel(this);
    QAction *zoomIn = new QAction(QIcon::fromTheme("zoom-in"), i18n("Zoom In"));
    QAction *zoomOut = new QAction(QIcon::fromTheme("zoom-out"), i18n("Zoom Out"));
    QAction *importAction = new QAction(QIcon::fromTheme("document-open"), i18n("Import Selection"));
    connect(importAction, &QAction::triggered, this, [this]() { importSelection(); });
    QAction *importOnDoubleClick = new QAction(QIcon::fromTheme("document-open"), i18n("Import File on Double Click"));
    importOnDoubleClick->setCheckable(true);
    importOnDoubleClick->setChecked(KdenliveSettings::mediaDoubleClickImport());
    connect(importOnDoubleClick, &QAction::triggered, this, [](bool enabled) { KdenliveSettings::setMediaDoubleClickImport(enabled); });

    // Create View
    m_op = new KDirOperator(QUrl(), parent);
    m_op->dirLister()->setAutoUpdate(false);
    // Ensure shortcuts are only active on this widget to avoid conflicts with app shortcuts
#if KIO_VERSION >= QT_VERSION_CHECK(5, 100, 0)
    QList<QAction *> actions = m_op->allActions();
    QAction *trash = m_op->action(KDirOperator::Trash);
    QAction *up = m_op->action(KDirOperator::Up);
    QAction *back = m_op->action(KDirOperator::Back);
    QAction *forward = m_op->action(KDirOperator::Forward);
    QAction *inlinePreview = m_op->action(KDirOperator::ShowPreviewPanel);
    QAction *preview = m_op->action(KDirOperator::ShowPreviewPanel);
    QAction *viewMenu = m_op->action(KDirOperator::ViewModeMenu);
#else
    QList<QAction *> actions = m_op->actionCollection()->actions();
    QAction *trash = m_op->actionCollection()->action("trash");
    QAction *up = m_op->actionCollection()->action("up");
    QAction *back = m_op->actionCollection()->action("back");
    QAction *forward = m_op->actionCollection()->action("forward");
    QAction *inlinePreview = m_op->actionCollection()->action("inline preview");
    QAction *preview = m_op->actionCollection()->action("preview");
    QAction *viewMenu = m_op->actionCollection()->action("view menu");
#endif
    // Disable "Del" shortcut to move item to trash - too dangerous ??
    trash->setShortcut({});
    // Plug actions
    tb->addAction(up);
    tb->addAction(back);
    tb->addAction(forward);

    tb->addAction(zoomOut);
    tb->addAction(zoomIn);
    tb->addAction(inlinePreview);
    tb->addSeparator();
    tb->addAction(importAction);
    preview->setIcon(QIcon::fromTheme(QStringLiteral("fileview-preview")));

    // Menu button with extra actions
    QToolButton *but = new QToolButton(this);
    QMenu *configMenu = new QMenu(this);
    configMenu->addAction(importOnDoubleClick);
    configMenu->addAction(preview);
    configMenu->addAction(viewMenu);
    but->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    but->setMenu(configMenu);
    but->setPopupMode(QToolButton::InstantPopup);

    // Spacer
    QWidget *empty = new QWidget(this);
    empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(empty);
    tb->addWidget(but);

    connect(zoomIn, &QAction::triggered, this, [this]() {
        int iconZoom = m_op->iconSize();
        int newZoom = iconZoom * 1.5;
        m_op->setIconSize(qMin(512, newZoom));
    });
    connect(zoomOut, &QAction::triggered, this, [this]() {
        int iconZoom = m_op->iconSize() / 1.5;
        iconZoom = qMax(int(KIconLoader::SizeSmall), iconZoom);
        m_op->setIconSize(iconZoom);
    });
    KConfigGroup grp(KSharedConfig::openConfig(), "Media Browser");
    m_op->readConfig(grp);
#if KIO_VERSION >= QT_VERSION_CHECK(5, 100, 0)
    m_op->setViewMode(KFile::Default);
#else
    m_op->setView(KFile::Default);
#endif
    m_op->setMode(KFile::ExistingOnly | KFile::Files | KFile::Directory);
    m_op->setIconSize(KdenliveSettings::mediaIconSize());
    m_filterCombo = new KFileFilterCombo(this);
    m_filenameEdit = new KUrlComboBox(KUrlComboBox::Files, true, this);
    m_locationEdit = new KUrlNavigator(places, QUrl(), this);
    connect(m_locationEdit, &KUrlNavigator::urlChanged, this, [this](const QUrl url) { m_op->setUrl(url, true); });

    connect(m_op, &KDirOperator::urlEntered, this, &MediaBrowser::slotUrlEntered);
    connect(m_op, &KDirOperator::viewChanged, this, &MediaBrowser::connectView);
    connect(m_op, &KDirOperator::currentIconSizeChanged, this, [](int size) { KdenliveSettings::setMediaIconSize(size); });
    connect(m_op, &KDirOperator::fileHighlighted, this, [this](const KFileItem &item) {
        KFileItemList files = m_op->selectedItems();
        if (item.isDir() || files.size() == 0) {
            m_filenameEdit->clear();
        } else {
            const QString fileName = files.first().url().fileName();
            if (!fileName.isEmpty()) {
                m_filenameEdit->setUrl(QUrl(fileName));
            }
        }
    });
    connect(m_op, &KDirOperator::contextMenuAboutToShow, this, [importAction](const KFileItem &, QMenu *menu) {
        QList<QAction *> act = menu->actions();
        if (act.isEmpty()) {
            menu->addAction(importAction);
        } else if (!act.contains(importAction)) {
            menu->insertAction(act.first(), importAction);
        }
    });

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QString allExtensions = ClipCreationDialog::getExtensions().join(QLatin1Char(' '));
    QString dialogFilter = allExtensions + QLatin1Char('|') + i18n("All Supported Files") + QStringLiteral("\n*|") + i18n("All Files");
    m_filterCombo->setFilter(dialogFilter);
    m_op->setNameFilter(m_filterCombo->currentFilter());
#else
    QStringList allExtensions = ClipCreationDialog::getExtensions();

    const QList<KFileFilter> filters{
        KFileFilter(i18n("All Supported Files"), allExtensions, {}),
        KFileFilter(i18n("All Files"), {QStringLiteral("*")}, {}),
    };

    m_filterCombo->setFilters(filters, filters.first());
    m_op->setNameFilter(filters.first().toFilterString());
#endif

    // Setup mime filter combo
    m_filterCombo->setEditable(true);
    m_filterCombo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    connect(m_filterCombo, &KFileFilterCombo::filterChanged, this, [this]() { slotFilterChanged(); });
    m_filterDelayTimer.setSingleShot(true);
    m_filterDelayTimer.setInterval(300);
    connect(m_filterCombo, &QComboBox::editTextChanged, &m_filterDelayTimer, qOverload<>(&QTimer::start));
    connect(&m_filterDelayTimer, &QTimer::timeout, this, [this]() { slotFilterChanged(); });

    // Load initial folder
    m_op->setUrl(QUrl::fromLocalFile(clipFolder), true);

    // Setup file name field
    m_filenameEdit->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    connect(m_filenameEdit, &KUrlComboBox::editTextChanged, this, [this](const QString &text) { slotLocationChanged(text); });
    connect(m_filenameEdit, qOverload<const QString &>(&KUrlComboBox::returnPressed), this, [this](const QString &) {
        KFileItemList files = m_op->selectedItems();
        if (files.size() == 1 && files.first().isDir()) {
            m_op->setUrl(m_op->url().resolved(files.first().url()), true);
        } else {
            importSelection();
        }
    });
    KUrlCompletion *fileCompletionObj = new KUrlCompletion(KUrlCompletion::FileCompletion);
    fileCompletionObj->setDir(m_op->url());
    m_filenameEdit->setCompletionObject(fileCompletionObj);
    m_filenameEdit->setAutoDeleteCompletionObject(true);

    // Layout stuff
    // File name
    QHBoxLayout *hlay1 = new QHBoxLayout;
    hlay1->addWidget(new QLabel(i18n("Name:"), this));
    hlay1->addWidget(m_filenameEdit);
    // Mime filter
    QHBoxLayout *hlay2 = new QHBoxLayout;
    hlay2->addWidget(new QLabel(i18n("Filter:"), this));
    hlay2->addWidget(m_filterCombo);
    // Checkbox options
    QHBoxLayout *hlay3 = new QHBoxLayout;
    QCheckBox *b = new QCheckBox(i18n("Ignore subfolder structure"));
    b->setChecked(KdenliveSettings::ignoresubdirstructure());
    b->setToolTip(i18n("Do not create subfolders in Project Bin"));
    b->setWhatsThis(
        xi18nc("@info:whatsthis",
               "When enabled, Kdenlive will import all clips contained in the folder and its subfolders without creating the subfolders in Project Bin."));
    connect(b, &QCheckBox::toggled, this, [](bool enabled) { KdenliveSettings::setIgnoresubdirstructure(enabled); });

    QCheckBox *b2 = new QCheckBox(i18n("Import image sequence"));
    b2->setChecked(KdenliveSettings::autoimagesequence());
    connect(b2, &QCheckBox::toggled, this, [](bool enabled) { KdenliveSettings::setAutoimagesequence(enabled); });
    b2->setToolTip(i18n("Try to import an image sequence"));
    b2->setWhatsThis(
        xi18nc("@info:whatsthis", "When enabled, Kdenlive will look for other images with the same name pattern and import them as an image sequence."));
    hlay3->addWidget(b2);
    hlay3->addWidget(b);
    lay->addWidget(tb);
    lay->addWidget(m_locationEdit);
    lay->addWidget(m_op);
    lay->addLayout(hlay1);
    lay->addLayout(hlay2);
    lay->addLayout(hlay3);
    for (QObject *child : children()) {
        if (child->isWidgetType()) {
            child->installEventFilter(this);
        }
    }
    connectView();
}

MediaBrowser::~MediaBrowser()
{
    KConfigGroup grp(KSharedConfig::openConfig(), "Media Browser");
    m_op->writeConfig(grp);
}

void MediaBrowser::connectView()
{
    connect(m_op->view(), &QAbstractItemView::doubleClicked, this, &MediaBrowser::slotViewDoubleClicked);
    m_op->view()->installEventFilter(this);
    m_op->setInlinePreviewShown(true);

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    // Unconditionnaly enable video thumbnails on Windows/Mac as user can't edit the global KDE settings
    if (m_op->previewGenerator()) {
        QStringList enabledPlugs = m_op->previewGenerator()->enabledPlugins();
        if (!enabledPlugs.contains(QStringLiteral("ffmpegthumbs"))) {
            enabledPlugs << QStringLiteral("ffmpegthumbs");
            m_op->previewGenerator()->setEnabledPlugins(enabledPlugs);
        }
    }
#endif
    setFocusProxy(m_op->view());
    setFocusPolicy(Qt::ClickFocus);
}

void MediaBrowser::slotViewDoubleClicked()
{
    if (KdenliveSettings::mediaDoubleClickImport()) {
        importSelection();
        return;
    }
    KFileItemList files = m_op->selectedItems();
    if (files.isEmpty()) {
        return;
    }
    if (files.first().isDir()) {
        return;
    }
    openExternalFile(files.first().url());
}

void MediaBrowser::detectShortcutConflicts()
{
#if KIO_VERSION >= QT_VERSION_CHECK(5, 100, 0)
    QList<QAction *> actions = m_op->allActions();
#else
    QList<QAction *> actions = m_op->actionCollection()->actions();
#endif
    QList<QKeySequence> shortcutsList;
    m_browserActions.clear();
    m_conflictingAppActions.clear();
    for (auto &a : actions) {
        QKeySequence sh = a->shortcut();
        if (!sh.isEmpty()) {
            shortcutsList << sh;
            m_browserActions << a;
            a->setEnabled(false);
        }
    }
    // qDebug()<<"::: FOUND BROWSER SHORTCUTS: "<<shortcutsList<<"\n__________________________";
    QList<QAction *> appActions = pCore->window()->actionCollection()->actions();
    for (auto &a : appActions) {
        QKeySequence sh = a->shortcut();
        if (!sh.isEmpty() && shortcutsList.contains(sh)) {
            m_conflictingAppActions << a;
        }
    }
}

void MediaBrowser::importSelection()
{
    KFileItemList files = m_op->selectedItems();
    if (KdenliveSettings::autoimagesequence() && files.size() == 1) {
        QMimeDatabase db;
        QUrl url = files.first().url();
        QMimeType type = db.mimeTypeForUrl(url);

        QDomElement prod;
        qDebug() << "=== GOT DROPPED MIME: " << type.name();
        if (type.name().startsWith(QLatin1String("image/")) && !type.name().contains(QLatin1String("image/gif"))) {
            QStringList patternlist;
            QString pattern = SlideshowClip::selectedPath(url, false, QString(), &patternlist);
            Timecode tc = pCore->timecode();
            QScopedPointer<SlideshowClip> dia(new SlideshowClip(tc, url.toLocalFile(), nullptr, this));
            if (dia->exec() == QDialog::Accepted) {
                QString parentFolder = "-1";
                std::unordered_map<QString, QString> properties;
                properties[QStringLiteral("ttl")] = QString::number(tc.getFrameCount(dia->clipDuration()));
                properties[QStringLiteral("loop")] = QString::number(static_cast<int>(dia->loop()));
                properties[QStringLiteral("crop")] = QString::number(static_cast<int>(dia->crop()));
                properties[QStringLiteral("fade")] = QString::number(static_cast<int>(dia->fade()));
                properties[QStringLiteral("luma_duration")] = QString::number(tc.getFrameCount(dia->lumaDuration()));
                properties[QStringLiteral("luma_file")] = dia->lumaFile();
                properties[QStringLiteral("softness")] = QString::number(dia->softness());
                properties[QStringLiteral("animation")] = dia->animation();
                properties[QStringLiteral("low-pass")] = QString::number(dia->lowPass());
                int duration = tc.getFrameCount(dia->clipDuration()) * dia->imageCount();
                ClipCreator::createSlideshowClip(dia->selectedPath(), duration, dia->clipName(), parentFolder, properties, pCore->projectItemModel());
            }
            return;
        }
    }
    QList<QUrl> urls;
    for (auto &f : files) {
        urls << f.url();
    }
    pCore->bin()->droppedUrls(urls);
}

const QUrl MediaBrowser::url() const
{
    return m_op->url();
}

bool MediaBrowser::eventFilter(QObject *watched, QEvent *event)
{
    // To avoid shortcut conflicts between the media browser and main app, we dis/enable actions when we gain/lose focus
    if (event->type() == QEvent::FocusIn) {
        qDebug() << ":::::: \n\nFOCUS IN\n\n:::::::::::::::::";
        disableAppShortcuts();
    } else if (event->type() == QEvent::FocusOut) {
        qDebug() << ":::::: \n\nFOCUS OUT\n\n:::::::::::::::::";
        enableAppShortcuts();
    } else if (event->type() == QEvent::Hide) {
        if (m_op->dirLister()->autoUpdate()) {
            m_op->dirLister()->setAutoUpdate(false);
        }
    } else if (event->type() == QEvent::Show) {
        if (!m_op->dirLister()->autoUpdate()) {
            m_op->dirLister()->setAutoUpdate(true);
        }
    }
    const bool res = QWidget::eventFilter(watched, event);
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if (!keyEvent) {
        return res;
    }

    const auto type = event->type();
    const auto key = keyEvent->key();

    if (watched == m_op && type == QEvent::KeyPress && (key == Qt::Key_Return || key == Qt::Key_Enter)) {
        // ignore return events from the KDirOperator
        importSelection();
        event->accept();
        return true;
    }
    return res;
}

void MediaBrowser::disableAppShortcuts()
{
    for (auto &a : m_conflictingAppActions) {
        if (a) {
            a->setEnabled(false);
        }
    }
    for (auto &a : m_browserActions) {
        if (a) {
            a->setEnabled(true);
        }
    }
}

void MediaBrowser::enableAppShortcuts()
{
    for (auto &a : m_browserActions) {
        if (a) {
            a->setEnabled(false);
        }
    }
    for (auto &a : m_conflictingAppActions) {
        if (a) {
            a->setEnabled(true);
        }
    }
}

void MediaBrowser::slotUrlEntered(const QUrl &url)
{
    QSignalBlocker bk(m_locationEdit);
    m_locationEdit->setLocationUrl(url);
    QSignalBlocker bk2(m_filenameEdit);
    KUrlCompletion *completion = dynamic_cast<KUrlCompletion *>(m_filenameEdit->completionObject());
    if (completion) {
        completion->setDir(url);
    }
}

void MediaBrowser::setUrl(const QUrl url)
{
    m_op->setUrl(url, true);
    m_op->setIconSize(KdenliveSettings::mediaIconSize());
}

void MediaBrowser::slotFilterChanged()
{
    m_filterDelayTimer.stop();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QString filter = m_filterCombo->currentFilter();
    m_op->clearFilter();
    if (filter.contains(QLatin1Char('/'))) {
        QStringList types = filter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        types.prepend(QStringLiteral("inode/directory"));
        m_op->setMimeFilter(types);
    } else if (filter.contains(QLatin1Char('*')) || filter.contains(QLatin1Char('?')) || filter.contains(QLatin1Char('['))) {
        m_op->setNameFilter(filter);
    } else {
        m_op->setNameFilter(QLatin1Char('*') + filter.replace(QLatin1Char(' '), QLatin1Char('*')) + QLatin1Char('*'));
    }
#else
    KFileFilter filter = m_filterCombo->currentFilter();
    m_op->clearFilter();
    if (!filter.mimePatterns().isEmpty()) {
        QStringList types = filter.mimePatterns();
        types.prepend(QStringLiteral("inode/directory"));
        m_op->setMimeFilter(types);
    } else {
        m_op->setNameFilter(filter.toFilterString());
    }
#endif
    m_op->updateDir();
}

void MediaBrowser::slotLocationChanged(const QString &text)
{
    m_filenameEdit->lineEdit()->setModified(true);
    if (text.isEmpty() && m_op->view()) {
        m_op->view()->clearSelection();
    }

    if (!m_filenameEdit->lineEdit()->text().isEmpty()) {
        QUrl u = m_op->url().adjusted(QUrl::RemoveFilename);
        QUrl partial_url;
        partial_url.setPath(text);
        const QUrl finalUrl = u.resolved(partial_url);
        qDebug() << ":::: GOT FINAL URL: " << finalUrl << "\n_____________________";
        ;
        // const QList<QUrl> urlList(tokenize(text));
        m_op->setCurrentItem(finalUrl);
    }
}

void MediaBrowser::openExternalFile(const QUrl &url)
{
    if (pCore->packageType() == QStringLiteral("appimage")) {
        qDebug() << "::::: LAUNCHING APPIMAGE BROWSER.........";
        QProcess process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        qDebug() << "::: GOT ENV: " << env.value("LD_LIBRARY_PATH") << ", PATH: " << env.value("PATH") << "\n\nXDG:\n" << env.value("XDG_DATA_DIRS");
        QStringList libPath = env.value(QStringLiteral("LD_LIBRARY_PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
        QStringList updatedLDPath;
        for (auto &s : libPath) {
            if (!s.startsWith(QStringLiteral("/tmp/.mount_"))) {
                updatedLDPath << s;
            }
        }
        if (updatedLDPath.isEmpty()) {
            env.remove(QStringLiteral("LD_LIBRARY_PATH"));
        } else {
            env.insert(QStringLiteral("LD_LIBRARY_PATH"), updatedLDPath.join(QLatin1Char(':')));
        }
        // Path
        libPath = env.value(QStringLiteral("PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
        updatedLDPath.clear();
        for (auto &s : libPath) {
            if (!s.startsWith(QStringLiteral("/tmp/.mount_"))) {
                updatedLDPath << s;
            }
        }
        if (updatedLDPath.isEmpty()) {
            env.remove(QStringLiteral("PATH"));
        } else {
            env.insert(QStringLiteral("PATH"), updatedLDPath.join(QLatin1Char(':')));
        }
        // XDG
        libPath = env.value(QStringLiteral("XDG_DATA_DIRS")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
        updatedLDPath.clear();
        for (auto &s : libPath) {
            if (!s.startsWith(QStringLiteral("/tmp/.mount_"))) {
                updatedLDPath << s;
            }
        }
        if (updatedLDPath.isEmpty()) {
            env.remove(QStringLiteral("XDG_DATA_DIRS"));
        } else {
            env.insert(QStringLiteral("XDG_DATA_DIRS"), updatedLDPath.join(QLatin1Char(':')));
        }
        env.remove(QStringLiteral("QT_QPA_PLATFORM"));
        process.setProcessEnvironment(env);
        QString openPath = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
        process.setProgram(openPath.isEmpty() ? QStringLiteral("xdg-open") : openPath);
        process.setArguments({url.toLocalFile()});
        process.startDetached();
    } else {
        QDesktopServices::openUrl(url);
    }
}

void MediaBrowser::back()
{
    m_op->back();
}

void MediaBrowser::forward()
{
    m_op->forward();
}
