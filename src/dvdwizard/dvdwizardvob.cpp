/***************************************************************************
 *   Copyright (C) 2009 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "dvdwizardvob.h"
#include "dialogs/clipcreationdialog.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "timecode.h"
#include <mlt++/Mlt.h>

#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KIO/Global>
#include <KMessageBox>
#include <KRecentDirs>
#include <KSharedConfig>

#include <QAction>
#include <QDomDocument>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMimeData>
#include <QStandardPaths>
#include <QTreeWidgetItem>
#include <unistd.h>

DvdTreeWidget::DvdTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setAcceptDrops(true);
}

void DvdTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->setDropAction(Qt::CopyAction);
        event->setAccepted(true);
    } else {
        QTreeWidget::dragEnterEvent(event);
    }
}

void DvdTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void DvdTreeWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    emit addNewClip();
}

void DvdTreeWidget::dropEvent(QDropEvent *event)
{
    QList<QUrl> clips = event->mimeData()->urls();
    event->accept();
    emit addClips(clips);
}

DvdWizardVob::DvdWizardVob(QWidget *parent)
    : QWizardPage(parent)

{
    m_view.setupUi(this);
    m_view.button_add->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_view.button_delete->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    m_view.button_up->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    m_view.button_down->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    m_vobList = new DvdTreeWidget(this);
    auto *lay1 = new QVBoxLayout;
    lay1->setContentsMargins(0, 0, 0, 0);
    lay1->addWidget(m_vobList);
    m_view.list_frame->setLayout(lay1);
    m_vobList->setColumnCount(3);
    m_vobList->setHeaderHidden(true);
    m_view.convert_box->setVisible(false);

    connect(m_vobList, SIGNAL(addClips(QList<QUrl>)), this, SLOT(slotAddVobList(QList<QUrl>)));
    connect(m_vobList, SIGNAL(addNewClip()), this, SLOT(slotAddVobList()));
    connect(m_view.button_add, SIGNAL(clicked()), this, SLOT(slotAddVobList()));
    connect(m_view.button_delete, &QAbstractButton::clicked, this, &DvdWizardVob::slotDeleteVobFile);
    connect(m_view.button_up, &QAbstractButton::clicked, this, &DvdWizardVob::slotItemUp);
    connect(m_view.button_down, &QAbstractButton::clicked, this, &DvdWizardVob::slotItemDown);
    connect(m_view.convert_abort, &QAbstractButton::clicked, this, &DvdWizardVob::slotAbortTranscode);
    connect(m_vobList, &QTreeWidget::itemSelectionChanged, this, &DvdWizardVob::slotCheckVobList);

    m_vobList->setIconSize(QSize(60, 45));

    QString errorMessage;
    if (QStandardPaths::findExecutable(QStringLiteral("dvdauthor")).isEmpty()) {
        errorMessage.append(i18n("<strong>Program %1 is required for the DVD wizard.</strong>", i18n("dvdauthor")));
    }
    if (QStandardPaths::findExecutable(QStringLiteral("mkisofs")).isEmpty() && QStandardPaths::findExecutable(QStringLiteral("genisoimage")).isEmpty()) {
        errorMessage.append(i18n("<strong>Program %1 or %2 is required for the DVD wizard.</strong>", i18n("mkisofs"), i18n("genisoimage")));
    }
    if (!errorMessage.isEmpty()) {
        m_view.button_add->setEnabled(false);
        m_view.dvd_profile->setEnabled(false);
    }

    m_view.dvd_profile->addItems(QStringList() << i18n("PAL 4:3") << i18n("PAL 16:9") << i18n("NTSC 4:3") << i18n("NTSC 16:9"));

    connect(m_view.dvd_profile, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &DvdWizardVob::slotCheckProfiles);
    m_vobList->header()->setStretchLastSection(false);
    m_vobList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_vobList->header()->setSectionResizeMode(1, QHeaderView::Custom);
    m_vobList->header()->setSectionResizeMode(2, QHeaderView::Custom);

    m_capacityBar = new KCapacityBar(KCapacityBar::DrawTextInline, this);
    auto *lay = new QHBoxLayout;
    lay->addWidget(m_capacityBar);
    m_view.size_box->setLayout(lay);

    m_vobList->setItemDelegate(new DvdViewDelegate(m_vobList));
    m_transcodeAction = new QAction(i18n("Transcode"), this);
    connect(m_transcodeAction, &QAction::triggered, this, &DvdWizardVob::slotTranscodeFiles);

    m_warnMessage = new KMessageWidget;
    m_warnMessage->setCloseButtonVisible(false);
    auto *s = static_cast<QGridLayout *>(layout());
    s->addWidget(m_warnMessage, 2, 0, 1, -1);
    if (!errorMessage.isEmpty()) {
        m_warnMessage->setMessageType(KMessageWidget::Error);
        m_warnMessage->setText(errorMessage);
        m_installCheck = false;
    } else {
        m_warnMessage->setMessageType(KMessageWidget::Warning);
        m_warnMessage->setText(i18n("Your clips do not match selected DVD format, transcoding required."));
        m_warnMessage->addAction(m_transcodeAction);
        m_warnMessage->hide();
    }
    m_view.button_transcode->setHidden(true);
    slotCheckVobList();

    m_transcodeProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_transcodeProcess, &QProcess::readyReadStandardOutput, this, &DvdWizardVob::slotShowTranscodeInfo);
    connect(&m_transcodeProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &DvdWizardVob::slotTranscodeFinished);
}

DvdWizardVob::~DvdWizardVob()
{
    delete m_capacityBar;
    // Abort running transcoding
    if (m_transcodeProcess.state() != QProcess::NotRunning) {
        disconnect(&m_transcodeProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this,
                   &DvdWizardVob::slotTranscodeFinished);
        m_transcodeProcess.close();
        m_transcodeProcess.waitForFinished();
    }
}

bool DvdWizardVob::isComplete() const
{
    return m_vobList->topLevelItemCount() > 0;
}

void DvdWizardVob::slotShowTranscodeInfo()
{
    QString log = QString(m_transcodeProcess.readAll());
    if (m_duration == 0) {
        if (log.contains(QStringLiteral("Duration:"))) {
            QString durationstr = log.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
            const QStringList numbers = durationstr.split(QLatin1Char(':'));
            if (numbers.size() < 3) {
                return;
            }
            m_duration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
            // log_text->setHidden(true);
            // job_progress->setHidden(false);
        } else {
            // log_text->setHidden(false);
            // job_progress->setHidden(true);
        }
    } else if (log.contains(QStringLiteral("time="))) {
        int progress;
        QString time = log.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
        if (time.contains(QLatin1Char(':'))) {
            const QStringList numbers = time.split(QLatin1Char(':'));
            if (numbers.size() < 3) {
                return;
            }
            progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
        } else {
            progress = (int)time.toDouble();
        }
        m_view.convert_progress->setValue((int)(100.0 * progress / m_duration));
    }
    // log_text->setPlainText(log);
}

void DvdWizardVob::slotAbortTranscode()
{
    if (m_transcodeProcess.state() != QProcess::NotRunning) {
        m_transcodeProcess.close();
        m_transcodeProcess.waitForFinished();
    }
    m_transcodeQueue.clear();
    m_view.convert_box->hide();
    slotCheckProfiles();
}

void DvdWizardVob::slotTranscodeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        slotTranscodedClip(m_currentTranscoding.filename,
                           m_currentTranscoding.filename + m_currentTranscoding.params.section(QStringLiteral("%1"), 1, 1).section(QLatin1Char(' '), 0, 0));
        if (!m_transcodeQueue.isEmpty()) {
            m_transcodeProcess.close();
            processTranscoding();
            return;
        }
    } else {
        // Something failed
        // TODO show log
        m_warnMessage->setMessageType(KMessageWidget::Warning);
        m_warnMessage->setText(i18n("Transcoding failed"));
        m_warnMessage->animatedShow();
        m_transcodeQueue.clear();
    }
    m_duration = 0;
    m_transcodeProcess.close();
    if (m_transcodeQueue.isEmpty()) {
        m_view.convert_box->setHidden(true);
        slotCheckProfiles();
    }
}

void DvdWizardVob::slotCheckProfiles()
{
    bool conflict = false;
    int comboProfile = m_view.dvd_profile->currentIndex();
    for (int i = 0; i < m_vobList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item->data(0, Qt::UserRole + 1).toInt() != comboProfile) {
            conflict = true;
            break;
        }
    }
    m_transcodeAction->setEnabled(conflict);
    if (conflict) {
        showProfileError();
    } else {
        m_warnMessage->animatedHide();
    }
}

void DvdWizardVob::slotAddVobList(QList<QUrl> list)
{
    if (list.isEmpty()) {
        auto extraFilters = QStringList() << i18n("MPEG Files") + QStringLiteral(" (*.mpeg *.mpg *.vob)") << i18n("All Files") + QStringLiteral(" (*.*)");
        const QString dialogFilter = ClipCreationDialog::getExtensionsFilter(extraFilters);
        list = QFileDialog::getOpenFileUrls(this, i18n("Add new video file"), QUrl::fromLocalFile(KRecentDirs::dir(QStringLiteral(":KdenliveDvdFolder"))),
                                            dialogFilter);
        if (!list.isEmpty()) {
            KRecentDirs::add(QStringLiteral(":KdenliveDvdFolder"), list.at(0).adjusted(QUrl::RemoveFilename).toLocalFile());
        }
    }
    for (const QUrl &url : qAsConst(list)) {
        slotAddVobFile(url, QString(), false);
    }
    slotCheckVobList();
    slotCheckProfiles();
}

void DvdWizardVob::slotAddVobFile(const QUrl &url, const QString &chapters, bool checkFormats)
{
    if (!url.isValid()) {
        return;
    }
    QFile f(url.toLocalFile());
    qint64 fileSize = f.size();

    Mlt::Profile profile;
    profile.set_explicit(false);
    QTreeWidgetItem *item =
        new QTreeWidgetItem(m_vobList, QStringList() << url.toLocalFile() << QString() << QString::number(static_cast<KIO::filesize_t>(fileSize)));
    item->setData(2, Qt::UserRole, fileSize);
    item->setData(0, Qt::DecorationRole, QIcon::fromTheme(QStringLiteral("video-x-generic")).pixmap(60, 45));
    item->setToolTip(0, url.toLocalFile());

    QString resource = url.toLocalFile();
    resource.prepend(QStringLiteral("avformat:"));
    Mlt::Producer *producer = new Mlt::Producer(profile, resource.toUtf8().data());
    if ((producer != nullptr) && producer->is_valid() && !producer->is_blank()) {
        double fps = profile.fps();
        profile.from_producer(*producer);
        profile.set_explicit(1);
        if (!qFuzzyCompare(profile.fps(), fps)) {
            // fps changed, rebuild producer
            delete producer;
            producer = new Mlt::Producer(profile, resource.toUtf8().data());
        }
    }
    if ((producer != nullptr) && producer->is_valid() && !producer->is_blank()) {
        int width = 45.0 * profile.dar();
        if (width % 2 == 1) {
            width++;
        }
        item->setData(0, Qt::DecorationRole, QPixmap::fromImage(KThumb::getFrame(producer, 0, width, 45)));
        int playTime = producer->get_playtime();
        item->setText(1, Timecode::getStringTimecode(playTime, profile.fps()));
        item->setData(1, Qt::UserRole, playTime);
        int standard = -1;
        int aspect = profile.dar() * 100;
        if (profile.height() == 576 && qFuzzyCompare(profile.fps(), 25.0)) {
            if (aspect > 150) {
                standard = 1;
            } else {
                standard = 0;
            }
        } else if (profile.height() == 480 && qAbs(profile.fps() - 30000.0 / 1001) < 0.2) {
            if (aspect > 150) {
                standard = 3;
            } else {
                standard = 2;
            }
        }
        QString standardName;
        switch (standard) {
        case 3:
            standardName = i18n("NTSC 16:9");
            break;
        case 2:
            standardName = i18n("NTSC 4:3");
            break;
        case 1:
            standardName = i18n("PAL 16:9");
            break;
        case 0:
            standardName = i18n("PAL 4:3");
            break;
        default:
            standardName = i18n("Unknown");
        }
        standardName.append(QStringLiteral(" | %1x%2, %3fps").arg(profile.width()).arg(profile.height()).arg(profile.fps()));
        item->setData(0, Qt::UserRole, standardName);
        item->setData(0, Qt::UserRole + 1, standard);
        item->setData(0, Qt::UserRole + 2, QSize(profile.dar() * profile.height(), profile.height()));
        if (m_vobList->topLevelItemCount() == 1) {
            // This is the first added movie, auto select DVD format
            if (standard >= 0) {
                m_view.dvd_profile->blockSignals(true);
                m_view.dvd_profile->setCurrentIndex(standard);
                m_view.dvd_profile->blockSignals(false);
            }
        }

    } else {
        // Cannot load movie, reject
        showError(i18n("The clip %1 is invalid.", url.fileName()));
    }
    delete producer;

    if (!chapters.isEmpty()) {
        item->setData(1, Qt::UserRole + 1, chapters);
    } else if (QFile::exists(url.toLocalFile() + QStringLiteral(".dvdchapter"))) {
        // insert chapters as children
        QFile file(url.toLocalFile() + QStringLiteral(".dvdchapter"));
        if (file.open(QIODevice::ReadOnly)) {
            QDomDocument doc;
            if (!doc.setContent(&file)) {
                file.close();
                return;
            }
            file.close();
            QDomNodeList chapterNodes = doc.elementsByTagName(QStringLiteral("chapter"));
            QStringList chaptersList;
            for (int j = 0; j < chapterNodes.count(); ++j) {
                chaptersList.append(QString::number(chapterNodes.at(j).toElement().attribute(QStringLiteral("time")).toInt()));
            }
            item->setData(1, Qt::UserRole + 1, chaptersList.join(QLatin1Char(';')));
        }
    } else { // Explicitly add a chapter at 00:00:00:00
        item->setData(1, Qt::UserRole + 1, "0");
    }

    if (checkFormats) {
        slotCheckVobList();
        slotCheckProfiles();
    }
}

void DvdWizardVob::slotDeleteVobFile()
{
    QTreeWidgetItem *item = m_vobList->currentItem();
    if (item == nullptr) {
        return;
    }
    delete item;
    slotCheckVobList();
    slotCheckProfiles();
}

void DvdWizardVob::setUrl(const QString &url)
{
    slotAddVobFile(QUrl::fromLocalFile(url));
}

QStringList DvdWizardVob::selectedUrls() const
{
    QStringList result;
    int max = m_vobList->topLevelItemCount();
    int i = 0;
    if (m_view.use_intro->isChecked()) {
        // First movie is only for intro
        i = 1;
    }
    for (; i < max; ++i) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item) {
            result.append(item->text(0));
        }
    }
    return result;
}

QStringList DvdWizardVob::durations() const
{
    QStringList result;
    int max = m_vobList->topLevelItemCount();
    int i = 0;
    if (m_view.use_intro->isChecked()) {
        // First movie is only for intro
        i = 1;
    }
    for (; i < max; ++i) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item) {
            result.append(QString::number(item->data(1, Qt::UserRole).toInt()));
        }
    }
    return result;
}

QStringList DvdWizardVob::chapters() const
{
    QStringList result;
    int max = m_vobList->topLevelItemCount();
    int i = 0;
    if (m_view.use_intro->isChecked()) {
        // First movie is only for intro
        i = 1;
    }
    for (; i < max; ++i) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item) {
            result.append(item->data(1, Qt::UserRole + 1).toString());
        }
    }
    return result;
}

void DvdWizardVob::updateChapters(const QMap<QString, QString> &chaptersdata)
{
    int max = m_vobList->topLevelItemCount();
    int i = 0;
    if (m_view.use_intro->isChecked()) {
        // First movie is only for intro
        i = 1;
    }
    for (; i < max; ++i) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (chaptersdata.contains(item->text(0))) {
            item->setData(1, Qt::UserRole + 1, chaptersdata.value(item->text(0)));
        }
    }
}

int DvdWizardVob::duration(int ix) const
{
    int result = -1;
    QTreeWidgetItem *item = m_vobList->topLevelItem(ix);
    if (item) {
        result = item->data(1, Qt::UserRole).toInt();
    }
    return result;
}

const QString DvdWizardVob::introMovie() const
{
    QString url;
    if (m_view.use_intro->isChecked() && m_vobList->topLevelItemCount() > 0) {
        url = m_vobList->topLevelItem(0)->text(0);
    }
    return url;
}

void DvdWizardVob::setUseIntroMovie(bool use)
{
    m_view.use_intro->setChecked(use);
}

void DvdWizardVob::slotCheckVobList()
{
    emit completeChanged();
    int max = m_vobList->topLevelItemCount();
    QTreeWidgetItem *item = m_vobList->currentItem();
    bool hasItem = true;
    if (item == nullptr) {
        hasItem = false;
    }
    m_view.button_delete->setEnabled(hasItem);
    if (hasItem && m_vobList->indexOfTopLevelItem(item) == 0) {
        m_view.button_up->setEnabled(false);
    } else {
        m_view.button_up->setEnabled(hasItem);
    }
    if (hasItem && m_vobList->indexOfTopLevelItem(item) == max - 1) {
        m_view.button_down->setEnabled(false);
    } else {
        m_view.button_down->setEnabled(hasItem);
    }

    qint64 totalSize = 0;
    for (int i = 0; i < max; ++i) {
        item = m_vobList->topLevelItem(i);
        if (item) {
            totalSize += (qint64)item->data(2, Qt::UserRole).toInt();
        }
    }

    qint64 maxSize = (qint64)47000 * 100000;
    m_capacityBar->setValue(static_cast<int>(100 * totalSize / maxSize));
    m_capacityBar->setText(KIO::convertSize(static_cast<KIO::filesize_t>(totalSize)));
}

void DvdWizardVob::slotItemUp()
{
    QTreeWidgetItem *item = m_vobList->currentItem();
    if (item == nullptr) {
        return;
    }
    int index = m_vobList->indexOfTopLevelItem(item);
    if (index == 0) {
        return;
    }
    m_vobList->insertTopLevelItem(index - 1, m_vobList->takeTopLevelItem(index));
}

void DvdWizardVob::slotItemDown()
{
    int max = m_vobList->topLevelItemCount();
    QTreeWidgetItem *item = m_vobList->currentItem();
    if (item == nullptr) {
        return;
    }
    int index = m_vobList->indexOfTopLevelItem(item);
    if (index == max - 1) {
        return;
    }
    m_vobList->insertTopLevelItem(index + 1, m_vobList->takeTopLevelItem(index));
}

DVDFORMAT DvdWizardVob::dvdFormat() const
{
    return (DVDFORMAT)m_view.dvd_profile->currentIndex();
}

const QString DvdWizardVob::dvdProfile() const
{
    QString profile;
    switch (m_view.dvd_profile->currentIndex()) {
    case PAL_WIDE:
        profile = QStringLiteral("dv_pal_wide");
        break;
    case NTSC:
        profile = QStringLiteral("dv_ntsc");
        break;
    case NTSC_WIDE:
        profile = QStringLiteral("dv_ntsc_wide");
        break;
    default:
        profile = QStringLiteral("dv_pal");
    }
    return profile;
}

// static
QString DvdWizardVob::getDvdProfile(DVDFORMAT format)
{
    QString profile;
    switch (format) {
    case PAL_WIDE:
        profile = QStringLiteral("dv_pal_wide");
        break;
    case NTSC:
        profile = QStringLiteral("dv_ntsc");
        break;
    case NTSC_WIDE:
        profile = QStringLiteral("dv_ntsc_wide");
        break;
    default:
        profile = QStringLiteral("dv_pal");
    }
    return profile;
}

void DvdWizardVob::setProfile(const QString &profile)
{
    if (profile == QLatin1String("dv_pal_wide")) {
        m_view.dvd_profile->setCurrentIndex(PAL_WIDE);
    } else if (profile == QLatin1String("dv_ntsc")) {
        m_view.dvd_profile->setCurrentIndex(NTSC);
    } else if (profile == QLatin1String("dv_ntsc_wide")) {
        m_view.dvd_profile->setCurrentIndex(NTSC_WIDE);
    } else {
        m_view.dvd_profile->setCurrentIndex(PAL);
    }
}

void DvdWizardVob::clear()
{
    m_vobList->clear();
}

void DvdWizardVob::slotTranscodeFiles()
{
    m_warnMessage->animatedHide();
    // Find transcoding info related to selected DVD profile
    KSharedConfigPtr config =
        KSharedConfig::openConfig(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("kdenlivetranscodingrc")), KConfig::CascadeConfig);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    QString profileEasyName;
    QSize destSize;
    QSize finalSize;
    switch (m_view.dvd_profile->currentIndex()) {
    case PAL_WIDE:
        profileEasyName = QStringLiteral("DVD PAL 16:9");
        destSize = QSize(1024, 576);
        finalSize = QSize(720, 576);
        break;
    case NTSC:
        profileEasyName = QStringLiteral("DVD NTSC 4:3");
        destSize = QSize(640, 480);
        finalSize = QSize(720, 480);
        break;
    case NTSC_WIDE:
        profileEasyName = QStringLiteral("DVD NTSC 16:9");
        destSize = QSize(853, 480);
        finalSize = QSize(720, 480);
        break;
    default:
        profileEasyName = QStringLiteral("DVD PAL 4:3");
        destSize = QSize(768, 576);
        finalSize = QSize(720, 576);
    }
    QString params = transConfig.readEntry(profileEasyName);
    m_transcodeQueue.clear();
    m_view.convert_progress->setValue(0);
    m_duration = 0;
    // Transcode files that do not match selected profile
    int max = m_vobList->topLevelItemCount();
    int format = m_view.dvd_profile->currentIndex();
    m_view.convert_box->setVisible(true);
    for (int i = 0; i < max; ++i) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item->data(0, Qt::UserRole + 1).toInt() != format) {
            // File needs to be transcoded
            m_transcodeAction->setEnabled(false);
            QSize original = item->data(0, Qt::UserRole + 2).toSize();
            double input_aspect = (double)original.width() / original.height();
            QStringList postParams;
            if (input_aspect > (double)destSize.width() / destSize.height()) {
                // letterboxing
                int conv_height = (int)(destSize.width() / input_aspect);
                int conv_pad = (int)(((double)(destSize.height() - conv_height)) / 2.0);
                if (conv_pad % 2 == 1) {
                    conv_pad--;
                }
                postParams << QStringLiteral("-vf")
                           << QStringLiteral("scale=%1:%2,pad=%3:%4:0:%5,setdar=%6")
                                  .arg(finalSize.width())
                                  .arg(destSize.height() - 2 * conv_pad)
                                  .arg(finalSize.width())
                                  .arg(finalSize.height())
                                  .arg(conv_pad)
                                  .arg(input_aspect);
            } else {
                // pillarboxing
                int conv_width = (int)(destSize.height() * input_aspect);
                int conv_pad = (int)(((double)(destSize.width() - conv_width)) / destSize.width() * finalSize.width() / 2.0);
                if (conv_pad % 2 == 1) {
                    conv_pad--;
                }
                postParams << QStringLiteral("-vf")
                           << QStringLiteral("scale=%1:%2,pad=%3:%4:%5:0,setdar=%6")
                                  .arg(finalSize.width() - 2 * conv_pad)
                                  .arg(destSize.height())
                                  .arg(finalSize.width())
                                  .arg(finalSize.height())
                                  .arg(conv_pad)
                                  .arg(input_aspect);
            }
            TranscodeJobInfo jobInfo;
            jobInfo.filename = item->text(0);
            jobInfo.params = params.section(QLatin1Char(';'), 0, 0);
            jobInfo.postParams = postParams;
            m_transcodeQueue << jobInfo;
        }
    }
    processTranscoding();
}

void DvdWizardVob::processTranscoding()
{
    if (m_transcodeQueue.isEmpty()) {
        return;
    }
    m_currentTranscoding = m_transcodeQueue.takeFirst();
    QStringList parameters;
    QStringList postParams = m_currentTranscoding.postParams;
    QString params = m_currentTranscoding.params;
    QString extension = params.section(QStringLiteral("%1"), 1, 1).section(QLatin1Char(' '), 0, 0);
    parameters << QStringLiteral("-i") << m_currentTranscoding.filename;
    if (QFile::exists(m_currentTranscoding.filename + extension)) {
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", m_currentTranscoding.filename + extension)) ==
            KMessageBox::No) {
            // TODO inform about abortion
            m_transcodeQueue.clear();
            return;
        }
        parameters << QStringLiteral("-y");
    }

    bool replaceVfParams = false;
    const QStringList splitted = params.split(QLatin1Char(' '));
    for (QString s : splitted) {
        if (replaceVfParams) {
            parameters << postParams.at(1);
            replaceVfParams = false;
        } else if (s.startsWith(QLatin1String("%1"))) {
            parameters << s.replace(0, 2, m_currentTranscoding.filename);
        } else if (!postParams.isEmpty() && s == QLatin1String("-vf")) {
            replaceVfParams = true;
            parameters << s;
        } else {
            parameters << s;
        }
    }
    qCDebug(KDENLIVE_LOG) << " / / /STARTING TCODE JB: \n" << KdenliveSettings::ffmpegpath() << " = " << parameters;
    m_transcodeProcess.start(KdenliveSettings::ffmpegpath(), parameters);
    m_view.convert_label->setText(i18n("Transcoding: %1", QUrl::fromLocalFile(m_currentTranscoding.filename).fileName()));
}

void DvdWizardVob::slotTranscodedClip(const QString &src, const QString &transcoded)
{
    if (transcoded.isEmpty()) {
        // Transcoding canceled or failed
        m_transcodeAction->setEnabled(true);
        return;
    }
    int max = m_vobList->topLevelItemCount();
    for (int i = 0; i < max; ++i) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (QUrl::fromLocalFile(item->text(0)).toLocalFile() == src) {
            // Replace movie with transcoded version
            item->setText(0, transcoded);

            QFile f(transcoded);
            qint64 fileSize = f.size();

            Mlt::Profile profile;
            profile.set_explicit(false);
            item->setText(2, KIO::convertSize(static_cast<KIO::filesize_t>(fileSize)));
            item->setData(2, Qt::UserRole, fileSize);
            item->setData(0, Qt::DecorationRole, QIcon::fromTheme(QStringLiteral("video-x-generic")).pixmap(60, 45));
            item->setToolTip(0, transcoded);

            QString resource = transcoded;
            resource.prepend(QStringLiteral("avformat:"));
            Mlt::Producer *producer = new Mlt::Producer(profile, resource.toUtf8().data());
            if ((producer != nullptr) && producer->is_valid() && !producer->is_blank()) {
                double fps = profile.fps();
                profile.from_producer(*producer);
                profile.set_explicit(1);
                if (!qFuzzyCompare(profile.fps(), fps)) {
                    // fps changed, rebuild producer
                    delete producer;
                    producer = new Mlt::Producer(profile, resource.toUtf8().data());
                }
            }
            if ((producer != nullptr) && producer->is_valid() && !producer->is_blank()) {
                int width = 45.0 * profile.dar();
                if (width % 2 == 1) {
                    width++;
                }
                item->setData(0, Qt::DecorationRole, QPixmap::fromImage(KThumb::getFrame(producer, 0, width, 45)));
                int playTime = producer->get_playtime();
                item->setText(1, Timecode::getStringTimecode(playTime, profile.fps()));
                item->setData(1, Qt::UserRole, playTime);
                int standard = -1;
                int aspect = profile.dar() * 100;
                if (profile.height() == 576) {
                    if (aspect > 150) {
                        standard = 1;
                    } else {
                        standard = 0;
                    }
                } else if (profile.height() == 480) {
                    if (aspect > 150) {
                        standard = 3;
                    } else {
                        standard = 2;
                    }
                }
                QString standardName;
                switch (standard) {
                case 3:
                    standardName = i18n("NTSC 16:9");
                    break;
                case 2:
                    standardName = i18n("NTSC 4:3");
                    break;
                case 1:
                    standardName = i18n("PAL 16:9");
                    break;
                case 0:
                    standardName = i18n("PAL 4:3");
                    break;
                default:
                    standardName = i18n("Unknown");
                }
                item->setData(0, Qt::UserRole, standardName);
                item->setData(0, Qt::UserRole + 1, standard);
                item->setData(0, Qt::UserRole + 2, QSize(profile.dar() * profile.height(), profile.height()));
            } else {
                // Cannot load movie, reject
                showError(i18n("The clip %1 is invalid.", transcoded));
            }
            delete producer;
            slotCheckVobList();
            if (m_transcodeQueue.isEmpty()) {
                slotCheckProfiles();
            }
            break;
        }
    }
}

void DvdWizardVob::showProfileError()
{
    m_warnMessage->setText(i18n("Your clips do not match selected DVD format, transcoding required."));
    m_warnMessage->setCloseButtonVisible(false);
    m_warnMessage->addAction(m_transcodeAction);
    m_warnMessage->animatedShow();
}

void DvdWizardVob::showError(const QString &error)
{
    m_warnMessage->setText(error);
    m_warnMessage->setCloseButtonVisible(true);
    m_warnMessage->removeAction(m_transcodeAction);
    m_warnMessage->animatedShow();
}
