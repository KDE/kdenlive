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
#include "kthumb.h"
#include "timecode.h"
#include "cliptranscode.h"

#include <mlt++/Mlt.h>

#include <KUrlRequester>
#include <KDebug>
#include <KStandardDirs>
#include <KFileItem>
#include <KFileDialog>

#include <QHBoxLayout>
#include <QDomDocument>
#include <QTreeWidgetItem>
#include <QHeaderView>

DvdTreeWidget::DvdTreeWidget(QWidget *parent) :
        QTreeWidget(parent)
{
    setAcceptDrops(true);
}

void DvdTreeWidget::dragEnterEvent(QDragEnterEvent * event ) {
    if (event->mimeData()->hasUrls()) {
	event->setDropAction(Qt::CopyAction);
	event->setAccepted(true);
    }
    else QTreeWidget::dragEnterEvent(event);
}

void DvdTreeWidget::dragMoveEvent(QDragMoveEvent * event) {
       event->acceptProposedAction();
}

void DvdTreeWidget::mouseDoubleClickEvent( QMouseEvent * )
{
    emit addNewClip();
}

void DvdTreeWidget::dropEvent(QDropEvent * event ) {
    QList<QUrl> clips = event->mimeData()->urls();
    event->accept();
    emit addClips(clips);
}

DvdWizardVob::DvdWizardVob(QWidget *parent) :
        QWizardPage(parent),
        m_installCheck(true)
{
    m_view.setupUi(this);
    m_view.button_add->setIcon(KIcon("list-add"));
    m_view.button_delete->setIcon(KIcon("list-remove"));
    m_view.button_up->setIcon(KIcon("go-up"));
    m_view.button_down->setIcon(KIcon("go-down"));
    m_vobList = new DvdTreeWidget(this);
    QVBoxLayout *lay1 = new QVBoxLayout;
    lay1->addWidget(m_vobList);
    m_view.list_frame->setLayout(lay1);
    m_vobList->setColumnCount(3);
    m_vobList->setHeaderHidden(true);

    connect(m_vobList, SIGNAL(addClips(QList<QUrl>)), this, SLOT(slotAddVobList(QList<QUrl>)));
    connect(m_vobList, SIGNAL(addNewClip()), this, SLOT(slotAddVobFile()));
    connect(m_view.button_add, SIGNAL(clicked()), this, SLOT(slotAddVobFile()));
    connect(m_view.button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteVobFile()));
    connect(m_view.button_up, SIGNAL(clicked()), this, SLOT(slotItemUp()));
    connect(m_view.button_down, SIGNAL(clicked()), this, SLOT(slotItemDown()));
    connect(m_vobList, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckVobList()));
    
    m_vobList->setIconSize(QSize(60, 45));

    if (KStandardDirs::findExe("dvdauthor").isEmpty()) m_errorMessage.append(i18n("<strong>Program %1 is required for the DVD wizard.</strong>", i18n("dvdauthor")));
    if (KStandardDirs::findExe("mkisofs").isEmpty() && KStandardDirs::findExe("genisoimage").isEmpty()) m_errorMessage.append(i18n("<strong>Program %1 or %2 is required for the DVD wizard.</strong>", i18n("mkisofs"), i18n("genisoimage")));
    if (m_errorMessage.isEmpty()) m_view.error_message->setVisible(false);
    else {
	m_view.error_message->setText(m_errorMessage);
	m_installCheck = false;
    }

    m_view.dvd_profile->addItems(QStringList() << i18n("PAL 4:3") << i18n("PAL 16:9") << i18n("NTSC 4:3") << i18n("NTSC 16:9"));

    connect(m_view.dvd_profile, SIGNAL(activated(int)), this, SLOT(slotCheckProfiles()));
    m_vobList->header()->setStretchLastSection(false);
    m_vobList->header()->setResizeMode(0, QHeaderView::Stretch);
    m_vobList->header()->setResizeMode(1, QHeaderView::Custom);
    m_vobList->header()->setResizeMode(2, QHeaderView::Custom);

    m_capacityBar = new KCapacityBar(KCapacityBar::DrawTextInline, this);
    QHBoxLayout *lay = new QHBoxLayout;
    lay->addWidget(m_capacityBar);
    m_view.size_box->setLayout(lay);

    m_vobList->setItemDelegate(new DvdViewDelegate(m_vobList));
    m_transcodeAction = new QAction(i18n("Transcode"), this);
    connect(m_transcodeAction, SIGNAL(triggered()), this, SLOT(slotTranscodeFiles()));

#if KDE_IS_VERSION(4,7,0)
    m_warnMessage = new KMessageWidget;
    m_warnMessage->setMessageType(KMessageWidget::Warning);
    m_warnMessage->setText(i18n("Your clips do not match selected DVD format, transcoding required."));
    m_warnMessage->setCloseButtonVisible(false);
    m_warnMessage->addAction(m_transcodeAction);
    QGridLayout *s =  static_cast <QGridLayout*> (layout());
    s->addWidget(m_warnMessage, 3, 0, 1, -1);
    m_warnMessage->hide();
    m_view.button_transcode->setHidden(true);
#else
    m_view.button_transcode->setDefaultAction(m_transcodeAction);
    m_view.button_transcode->setEnabled(false);
#endif
    
    slotCheckVobList();
}

DvdWizardVob::~DvdWizardVob()
{
    delete m_capacityBar;
}

void DvdWizardVob::slotCheckProfiles()
{
    bool conflict = false;
    int comboProfile = m_view.dvd_profile->currentIndex();
    for (int i = 0; i < m_vobList->topLevelItemCount(); i++) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item->data(0, Qt::UserRole + 1).toInt() != comboProfile) {
	    conflict = true;
	    break;
	}
    }
    m_transcodeAction->setEnabled(conflict);
    if (conflict) {
	showProfileError();
    }
    else {
#if KDE_IS_VERSION(4,7,0)      
	m_warnMessage->animatedHide();
#else
	if (m_installCheck) m_view.error_message->setVisible(false);
#endif
    }
}

void DvdWizardVob::slotAddVobList(QList <QUrl>list)
{
    foreach (const QUrl url, list) {
	slotAddVobFile(KUrl(url), QString(), false);
    }
    slotCheckVobList();
    slotCheckProfiles();
}

void DvdWizardVob::slotAddVobFile(KUrl url, const QString &chapters, bool checkFormats)
{
    if (url.isEmpty()) url = KFileDialog::getOpenUrl(KUrl("kfiledialog:///projectfolder"), "video/mpeg", this, i18n("Add new video file"));
    if (url.isEmpty()) return;
    QFile f(url.path());
    qint64 fileSize = f.size();

    Mlt::Profile profile;
    profile.set_explicit(false);
    QTreeWidgetItem *item = new QTreeWidgetItem(m_vobList, QStringList() << url.path() << QString() << KIO::convertSize(fileSize));
    item->setData(2, Qt::UserRole, fileSize);
    item->setData(0, Qt::DecorationRole, KIcon("video-x-generic").pixmap(60, 45));
    item->setToolTip(0, url.path());

    QString resource = url.path();
    resource.prepend("avformat:");
    Mlt::Producer *producer = new Mlt::Producer(profile, resource.toUtf8().data());
    if (producer && producer->is_valid() && !producer->is_blank()) {
	//Mlt::Frame *frame = producer->get_frame();
	//delete frame;
	profile.from_producer(*producer);
        int width = 45.0 * profile.dar();
        int swidth = 45.0 * profile.width() / profile.height();
        if (width % 2 == 1) width++;
	item->setData(0, Qt::DecorationRole, QPixmap::fromImage(KThumb::getFrame(producer, 0, swidth, width, 45)));
        int playTime = producer->get_playtime();
        item->setText(1, Timecode::getStringTimecode(playTime, profile.fps()));
        item->setData(1, Qt::UserRole, playTime);
	int standard = -1;
	int aspect = profile.dar() * 100;
	if (profile.height() == 576) {
	    if (aspect > 150) standard = 1;
	    else standard = 0;
	}
	else if (profile.height() == 480) {
	    if (aspect > 150) standard = 3;
	    else standard = 2;
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
	if (m_vobList->topLevelItemCount() == 1) {
	    // This is the first added movie, auto select DVD format
	    if (standard >= 0) {
		m_view.dvd_profile->blockSignals(true);
		m_view.dvd_profile->setCurrentIndex(standard);
		m_view.dvd_profile->blockSignals(false);
	    }
	}
	
    }
    else {
	// Cannot load movie, reject
	showError(i18n("The clip %1 is invalid.", url.fileName()));
    }
    if (producer) delete producer;

    if (chapters.isEmpty() == false) {
        item->setData(1, Qt::UserRole + 1, chapters);
    }
    else if (QFile::exists(url.path() + ".dvdchapter")) {
        // insert chapters as children
        QFile file(url.path() + ".dvdchapter");
        if (file.open(QIODevice::ReadOnly)) {
            QDomDocument doc;
            if (doc.setContent(&file) == false) {
                file.close();
                return;
            }
            file.close();
            QDomNodeList chapters = doc.elementsByTagName("chapter");
            QStringList chaptersList;
            for (int j = 0; j < chapters.count(); j++) {
                chaptersList.append(QString::number(chapters.at(j).toElement().attribute("time").toInt()));
            }
            item->setData(1, Qt::UserRole + 1, chaptersList.join(";"));
        }
    } else // Explicitly add a chapter at 00:00:00:00
        item->setData(1, Qt::UserRole + 1, "0");

    if (checkFormats) {
	slotCheckVobList();
	slotCheckProfiles();
    }
}

void DvdWizardVob::slotDeleteVobFile()
{
    QTreeWidgetItem *item = m_vobList->currentItem();
    if (item == NULL) return;
    delete item;
    slotCheckVobList();
    slotCheckProfiles();
}


// virtual
bool DvdWizardVob::isComplete() const
{
    if (!m_installCheck) return false;
    if (m_vobList->topLevelItemCount() == 0) return false;
    return true;
}

void DvdWizardVob::setUrl(const QString &url)
{
    slotAddVobFile(KUrl(url));
}

QStringList DvdWizardVob::selectedUrls() const
{
    QStringList result;
    QString path;
    int max = m_vobList->topLevelItemCount();
    int i = 0;
    if (m_view.use_intro->isChecked()) {
	// First movie is only for intro
	i = 1;
    }
    for (; i < max; i++) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item) result.append(item->text(0));
    }
    return result;
}


QStringList DvdWizardVob::durations() const
{
    QStringList result;
    QString path;
    int max = m_vobList->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item) result.append(QString::number(item->data(1, Qt::UserRole).toInt()));
    }
    return result;
}

QStringList DvdWizardVob::chapters() const
{
    QStringList result;
    QString path;
    int max = m_vobList->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (item) {
            result.append(item->data(1, Qt::UserRole + 1).toString());
        }
    }
    return result;
}

void DvdWizardVob::updateChapters(QMap <QString, QString> chaptersdata)
{
    int max = m_vobList->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
        if (chaptersdata.contains(item->text(0))) item->setData(1, Qt::UserRole + 1, chaptersdata.value(item->text(0)));
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
    if (m_view.use_intro->isChecked() && m_vobList->topLevelItemCount() > 0) url = m_vobList->topLevelItem(0)->text(0);
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
    if (item == NULL) hasItem = false;
    m_view.button_delete->setEnabled(hasItem);
    if (hasItem && m_vobList->indexOfTopLevelItem(item) == 0) m_view.button_up->setEnabled(false);
    else m_view.button_up->setEnabled(hasItem);
    if (hasItem && m_vobList->indexOfTopLevelItem(item) == max - 1) m_view.button_down->setEnabled(false);
    else m_view.button_down->setEnabled(hasItem);

    qint64 totalSize = 0;
    for (int i = 0; i < max; i++) {
        item = m_vobList->topLevelItem(i);
        if (item) totalSize += (qint64) item->data(2, Qt::UserRole).toInt();
    }

    qint64 maxSize = (qint64) 47000 * 100000;
    m_capacityBar->setValue(100 * totalSize / maxSize);
    m_capacityBar->setText(KIO::convertSize(totalSize));
}

void DvdWizardVob::slotItemUp()
{
    QTreeWidgetItem *item = m_vobList->currentItem();
    if (item == NULL) return;
    int index = m_vobList->indexOfTopLevelItem(item);
    if (index == 0) return;
    m_vobList->insertTopLevelItem(index - 1, m_vobList->takeTopLevelItem(index));
}

void DvdWizardVob::slotItemDown()
{
    int max = m_vobList->topLevelItemCount();
    QTreeWidgetItem *item = m_vobList->currentItem();
    if (item == NULL) return;
    int index = m_vobList->indexOfTopLevelItem(item);
    if (index == max - 1) return;
    m_vobList->insertTopLevelItem(index + 1, m_vobList->takeTopLevelItem(index));
}

DVDFORMAT DvdWizardVob::dvdFormat() const
{
    return (DVDFORMAT) m_view.dvd_profile->currentIndex();
}

const QString DvdWizardVob::dvdProfile() const
{
    QString profile;
    switch (m_view.dvd_profile->currentIndex()) {
	case PAL_WIDE:
	    profile = "dv_pal_wide";
	    break;
	case NTSC:
	    profile = "dv_ntsc";
	    break;
	case NTSC_WIDE:
	    profile = "dv_ntsc_wide";
	    break;
	default:
	    profile = "dv_pal";
    }
    return profile;
}

//static
QString DvdWizardVob::getDvdProfile(DVDFORMAT format)
{
    QString profile;
    switch (format) {
	case PAL_WIDE:
	    profile = "dv_pal_wide";
	    break;
	case NTSC:
	    profile = "dv_ntsc";
	    break;
	case NTSC_WIDE:
	    profile = "dv_ntsc_wide";
	    break;
	default:
	    profile = "dv_pal";
    }
    return profile;
}

void DvdWizardVob::setProfile(const QString& profile)
{
    if (profile == "dv_pal_wide") m_view.dvd_profile->setCurrentIndex(PAL_WIDE);
    else if (profile == "dv_ntsc") m_view.dvd_profile->setCurrentIndex(NTSC);
    else if (profile == "dv_ntsc_wide") m_view.dvd_profile->setCurrentIndex(NTSC_WIDE);
    else m_view.dvd_profile->setCurrentIndex(PAL);
}

void DvdWizardVob::clear()
{
    m_vobList->clear();
}

void DvdWizardVob::slotTranscodeFiles()
{
    // Find transcoding infos related to selected DVD profile
    KSharedConfigPtr config = KSharedConfig::openConfig("kdenlivetranscodingrc", KConfig::CascadeConfig);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    QString profileEasyName;
    QSize destSize;
    QSize finalSize;
    switch (m_view.dvd_profile->currentIndex()) {
	case PAL_WIDE:
	    profileEasyName = "DVD PAL 16:9";
	    destSize = QSize(1024, 576);
	    finalSize = QSize(720, 576);
	    break;
	case NTSC:
	    profileEasyName = "DVD NTSC 4:3";
	    destSize = QSize(640, 480);
	    finalSize = QSize(720, 480);
	    break;
	case NTSC_WIDE:
	    profileEasyName = "DVD NTSC 16:9";
	    destSize = QSize(853, 480);
	    finalSize = QSize(720, 480);
	    break;
	default:
	    profileEasyName = "DVD PAL 4:3";
	    destSize = QSize(768, 576);
	    finalSize = QSize(720, 576);
    }
    QString params = transConfig.readEntry(profileEasyName);    
  
    // Transcode files that do not match selected profile
    int max = m_vobList->topLevelItemCount();
    int format = m_view.dvd_profile->currentIndex();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
	if (item->data(0, Qt::UserRole + 1).toInt() != format) {
	    // File needs to be transcoded
	    m_transcodeAction->setEnabled(false);
	    QSize original = item->data(0, Qt::UserRole + 2).toSize();
	    double input_aspect= (double) original.width() / original.height();
	    QStringList postParams;
	    if (input_aspect > (double) destSize.width() / destSize.height()) {
		// letterboxing
		int conv_height = (int) (destSize.width() / input_aspect);
		int conv_pad = (int) (((double) (destSize.height() - conv_height)) / 2.0);
		if (conv_pad %2 == 1) conv_pad --;
		postParams << "-vf" << QString("scale=%1:%2,pad=%3:%4:0:%5,setdar=%6").arg(finalSize.width()).arg(destSize.height() - 2 * conv_pad).arg(finalSize.width()).arg(finalSize.height()).arg(conv_pad).arg(input_aspect);
	    } else {
		// pillarboxing
		int conv_width = (int) (destSize.height() * input_aspect);
		int conv_pad = (int) (((double) (destSize.width() - conv_width)) / destSize.width() * finalSize.width() / 2.0);
		if (conv_pad %2 == 1) conv_pad --;
		postParams << "-vf" << QString("scale=%1:%2,pad=%3:%4:%5:0,setdar=%6").arg(finalSize.width() - 2 * conv_pad).arg(destSize.height()).arg(finalSize.width()).arg(finalSize.height()).arg(conv_pad).arg(input_aspect);
	    }
	    ClipTranscode *d = new ClipTranscode(KUrl::List () << KUrl(item->text(0)), params.section(';', 0, 0), postParams, i18n("Transcoding to DVD format"), true, this);
	    connect(d, SIGNAL(transcodedClip(KUrl,KUrl)), this, SLOT(slotTranscodedClip(KUrl, KUrl)));
	    d->show();
	}
    }
}

void DvdWizardVob::slotTranscodedClip(KUrl src, KUrl transcoded)
{
    int max = m_vobList->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_vobList->topLevelItem(i);
	if (KUrl(item->text(0)).path() == src.path()) {
	    // Replace movie with transcoded version
	    item->setText(0, transcoded.path());

	    QFile f(transcoded.path());
	    qint64 fileSize = f.size();

	    Mlt::Profile profile;
	    profile.set_explicit(false);
	    item->setText(2, KIO::convertSize(fileSize));
	    item->setData(2, Qt::UserRole, fileSize);
	    item->setData(0, Qt::DecorationRole, KIcon("video-x-generic").pixmap(60, 45));
	    item->setToolTip(0, transcoded.path());

	    QString resource = transcoded.path();
	    resource.prepend("avformat:");
	    Mlt::Producer *producer = new Mlt::Producer(profile, resource.toUtf8().data());
	    if (producer && producer->is_valid() && !producer->is_blank()) {
		profile.from_producer(*producer);
		int width = 45.0 * profile.dar();
		int swidth = 45.0 * profile.width() / profile.height();
		if (width % 2 == 1) width++;
		item->setData(0, Qt::DecorationRole, QPixmap::fromImage(KThumb::getFrame(producer, 0, swidth, width, 45)));
		int playTime = producer->get_playtime();
		item->setText(1, Timecode::getStringTimecode(playTime, profile.fps()));
		item->setData(1, Qt::UserRole, playTime);
		int standard = -1;
		int aspect = profile.dar() * 100;
		if (profile.height() == 576) {
		    if (aspect > 150) standard = 1;
		    else standard = 0;
		}
		else if (profile.height() == 480) {
		    if (aspect > 150) standard = 3;
		    else standard = 2;
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
	    }
	    else {
		// Cannot load movie, reject
		showError(i18n("The clip %1 is invalid.", transcoded.fileName()));
	    }
	    if (producer) delete producer;
	    slotCheckVobList();
	    slotCheckProfiles();
	    break;
	}
    }
}

void DvdWizardVob::showProfileError()
{
#if KDE_IS_VERSION(4,7,0)
    m_warnMessage->setText(i18n("Your clips do not match selected DVD format, transcoding required."));
    m_warnMessage->setCloseButtonVisible(false);
    m_warnMessage->addAction(m_transcodeAction);
    m_warnMessage->animatedShow();
#else
    m_view.error_message->setText(i18n("Your clips do not match selected DVD format, transcoding required."));
    m_view.error_message->setVisible(true);
#endif
}

void DvdWizardVob::showError(const QString error)
{
#if KDE_IS_VERSION(4,7,0)
    m_warnMessage->setText(error);
    m_warnMessage->setCloseButtonVisible(true);
    m_warnMessage->removeAction(m_transcodeAction);
    m_warnMessage->animatedShow();
#else
    m_view.error_message->setText(error);
    m_view.error_message->setVisible(true);
#endif    
}