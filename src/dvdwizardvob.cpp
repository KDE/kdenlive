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

DvdWizardVob::DvdWizardVob(const QString &profile, QWidget *parent) :
        QWizardPage(parent)
{
    m_view.setupUi(this);
    m_view.intro_vob->setEnabled(false);
    m_view.intro_vob->setFilter("video/mpeg");
    m_view.button_add->setIcon(KIcon("document-new"));
    m_view.button_delete->setIcon(KIcon("edit-delete"));
    m_view.button_up->setIcon(KIcon("go-up"));
    m_view.button_down->setIcon(KIcon("go-down"));
    connect(m_view.use_intro, SIGNAL(toggled(bool)), m_view.intro_vob, SLOT(setEnabled(bool)));
    connect(m_view.button_add, SIGNAL(clicked()), this, SLOT(slotAddVobFile()));
    connect(m_view.button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteVobFile()));
    connect(m_view.button_up, SIGNAL(clicked()), this, SLOT(slotItemUp()));
    connect(m_view.button_down, SIGNAL(clicked()), this, SLOT(slotItemDown()));
    connect(m_view.vobs_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckVobList()));
    
    m_view.vobs_list->setIconSize(QSize(60, 45));

    if (KStandardDirs::findExe("dvdauthor").isEmpty()) m_errorMessage.append(i18n("<strong>Program %1 is required for the DVD wizard.</strong>", i18n("dvdauthor")));
    if (KStandardDirs::findExe("mkisofs").isEmpty() && KStandardDirs::findExe("genisoimage").isEmpty()) m_errorMessage.append(i18n("<strong>Program %1 or %2 is required for the DVD wizard.</strong>", i18n("mkisofs"), i18n("genisoimage")));
    if (m_errorMessage.isEmpty()) m_view.error_message->setVisible(false);
    else m_view.error_message->setText(m_errorMessage);

    m_view.dvd_profile->addItems(QStringList() << i18n("PAL 4:3") << i18n("PAL 16:9") << i18n("NTSC 4:3") << i18n("NTSC 16:9"));
    if (profile == "dv_pal_wide") m_view.dvd_profile->setCurrentIndex(1);
    else if (profile == "dv_ntsc") m_view.dvd_profile->setCurrentIndex(2);
    else if (profile == "dv_ntsc_wide") m_view.dvd_profile->setCurrentIndex(3);

    connect(m_view.dvd_profile, SIGNAL(activated(int)), this, SLOT(changeFormat()));
    connect(m_view.dvd_profile, SIGNAL(activated(int)), this, SLOT(slotCheckProfiles()));
    m_view.vobs_list->header()->setStretchLastSection(false);
    m_view.vobs_list->header()->setResizeMode(0, QHeaderView::Stretch);
    m_view.vobs_list->header()->setResizeMode(1, QHeaderView::Custom);
    m_view.vobs_list->header()->setResizeMode(2, QHeaderView::Custom);

    m_capacityBar = new KCapacityBar(KCapacityBar::DrawTextInline, this);
    QHBoxLayout *lay = new QHBoxLayout;
    lay->addWidget(m_capacityBar);
    m_view.size_box->setLayout(lay);

    m_view.vobs_list->setItemDelegate(new DvdViewDelegate(m_view.vobs_list));

#if KDE_IS_VERSION(4,7,0)
    m_warnMessage = new KMessageWidget;
    m_warnMessage->setText(i18n("Conflicting video standards, check DVD profile and clips"));
    m_warnMessage->setMessageType(KMessageWidget::Warning);
    QGridLayout *s =  static_cast <QGridLayout*> (layout());
    s->addWidget(m_warnMessage, 3, 0, 1, -1);
    m_warnMessage->hide();
#endif
    
    slotCheckVobList();
}

DvdWizardVob::~DvdWizardVob()
{
    delete m_capacityBar;
}

void DvdWizardVob::slotCheckProfiles()
{
#if KDE_IS_VERSION(4,7,0)
    bool conflict = false;
    int comboProfile = m_view.dvd_profile->currentIndex();
    for (int i = 0; i < m_view.vobs_list->topLevelItemCount(); i++) {
        QTreeWidgetItem *item = m_view.vobs_list->topLevelItem(i);
        if (item->data(0, Qt::UserRole + 1).toInt() != comboProfile) {
	    conflict = true;
	    break;
	}
    }

    if (conflict) {
	m_warnMessage->animatedShow();
    }
    else m_warnMessage->animatedHide();
#endif
}

void DvdWizardVob::slotAddVobFile(KUrl url, const QString &chapters)
{
    if (url.isEmpty()) url = KFileDialog::getOpenUrl(KUrl("kfiledialog:///projectfolder"), "video/mpeg", this, i18n("Add new video file"));
    if (url.isEmpty()) return;
    QFile f(url.path());
    qint64 fileSize = f.size();

    Mlt::Profile profile;
    profile.set_explicit(false);
    QTreeWidgetItem *item = new QTreeWidgetItem(m_view.vobs_list, QStringList() << url.path() << QString() << KIO::convertSize(fileSize));
    item->setData(0, Qt::UserRole, fileSize);
    item->setData(0, Qt::DecorationRole, KIcon("video-x-generic").pixmap(60, 45));
    item->setToolTip(0, url.path());
    
    Mlt::Producer *producer = new Mlt::Producer(profile, url.path().toUtf8().data());
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
	if (m_view.vobs_list->topLevelItemCount() == 1) {
	    // This is the first added movie, auto select DVD format
	    if (standard >= 0) {
		m_view.dvd_profile->blockSignals(true);
		m_view.dvd_profile->setCurrentIndex(standard);
		m_view.dvd_profile->blockSignals(false);
	    }
	}
	
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

    slotCheckVobList();
    slotCheckProfiles();
}

void DvdWizardVob::changeFormat()
{
    int max = m_view.vobs_list->topLevelItemCount();
    QString profilename;
    switch (m_view.dvd_profile->currentIndex()) {
    case 1:
        profilename = "dv_pal_wide";
        break;
    case 2:
        profilename = "dv_ntsc";
        break;
    case 3:
        profilename = "dv_ntsc_wide";
        break;
    default:
        profilename = "dv_pal";
        break;
    }

    Mlt::Profile profile(profilename.toUtf8().constData());
    QPixmap pix(180, 135);

    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_view.vobs_list->topLevelItem(i);
        Mlt::Producer *producer = new Mlt::Producer(profile, item->text(0).toUtf8().data());

        if (producer->is_blank() == false) {
            //pix = KThumb::getFrame(producer, 0, 135 * profile.dar(), 135);
            //item->setIcon(0, pix);
            item->setText(1, Timecode::getStringTimecode(producer->get_playtime(), profile.fps()));
        }
        delete producer;
        int submax = item->childCount();
        for (int j = 0; j < submax; j++) {
            QTreeWidgetItem *subitem = item->child(j);
            subitem->setText(1, Timecode::getStringTimecode(subitem->data(1, Qt::UserRole).toInt(), profile.fps()));
        }
    }
    slotCheckVobList();
}

void DvdWizardVob::slotDeleteVobFile()
{
    QTreeWidgetItem *item = m_view.vobs_list->currentItem();
    if (item == NULL) return;
    delete item;
    slotCheckVobList();
    slotCheckProfiles();
}


// virtual
bool DvdWizardVob::isComplete() const
{
    if (!m_view.error_message->text().isEmpty()) return false;
    if (m_view.vobs_list->topLevelItemCount() == 0) return false;
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
    int max = m_view.vobs_list->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_view.vobs_list->topLevelItem(i);
        if (item) result.append(item->text(0));
    }
    return result;
}


QStringList DvdWizardVob::durations() const
{
    QStringList result;
    QString path;
    int max = m_view.vobs_list->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_view.vobs_list->topLevelItem(i);
        if (item) result.append(QString::number(item->data(1, Qt::UserRole).toInt()));
    }
    return result;
}

QStringList DvdWizardVob::chapters() const
{
    QStringList result;
    QString path;
    int max = m_view.vobs_list->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_view.vobs_list->topLevelItem(i);
        if (item) {
            result.append(item->data(1, Qt::UserRole + 1).toString());
        }
    }
    return result;
}

void DvdWizardVob::updateChapters(QMap <QString, QString> chaptersdata)
{
    int max = m_view.vobs_list->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_view.vobs_list->topLevelItem(i);
        if (chaptersdata.contains(item->text(0))) item->setData(1, Qt::UserRole + 1, chaptersdata.value(item->text(0)));
    }
}

int DvdWizardVob::duration(int ix) const
{
    int result = -1;
    QTreeWidgetItem *item = m_view.vobs_list->topLevelItem(ix);
    if (item) {
        result = item->data(1, Qt::UserRole).toInt();
    }
    return result;
}


QString DvdWizardVob::introMovie() const
{
    if (!m_view.use_intro->isChecked()) return QString();
    return m_view.intro_vob->url().path();
}

void DvdWizardVob::setIntroMovie(const QString& path)
{
    m_view.intro_vob->setUrl(KUrl(path));
    m_view.use_intro->setChecked(path.isEmpty() == false);
}


void DvdWizardVob::slotCheckVobList()
{
    emit completeChanged();
    int max = m_view.vobs_list->topLevelItemCount();
    QTreeWidgetItem *item = m_view.vobs_list->currentItem();
    bool hasItem = true;
    if (item == NULL) hasItem = false;
    m_view.button_delete->setEnabled(hasItem);
    if (hasItem && m_view.vobs_list->indexOfTopLevelItem(item) == 0) m_view.button_up->setEnabled(false);
    else m_view.button_up->setEnabled(hasItem);
    if (hasItem && m_view.vobs_list->indexOfTopLevelItem(item) == max - 1) m_view.button_down->setEnabled(false);
    else m_view.button_down->setEnabled(hasItem);

    qint64 totalSize = 0;
    for (int i = 0; i < max; i++) {
        item = m_view.vobs_list->topLevelItem(i);
        if (item) totalSize += (qint64) item->data(0, Qt::UserRole).toInt();
    }

    qint64 maxSize = (qint64) 47000 * 100000;
    m_capacityBar->setValue(100 * totalSize / maxSize);
    m_capacityBar->setText(KIO::convertSize(totalSize));
}

void DvdWizardVob::slotItemUp()
{
    QTreeWidgetItem *item = m_view.vobs_list->currentItem();
    if (item == NULL) return;
    int index = m_view.vobs_list->indexOfTopLevelItem(item);
    if (index == 0) return;
    m_view.vobs_list->insertTopLevelItem(index - 1, m_view.vobs_list->takeTopLevelItem(index));
}

void DvdWizardVob::slotItemDown()
{
    int max = m_view.vobs_list->topLevelItemCount();
    QTreeWidgetItem *item = m_view.vobs_list->currentItem();
    if (item == NULL) return;
    int index = m_view.vobs_list->indexOfTopLevelItem(item);
    if (index == max - 1) return;
    m_view.vobs_list->insertTopLevelItem(index + 1, m_view.vobs_list->takeTopLevelItem(index));
}

bool DvdWizardVob::isPal() const
{
    return m_view.dvd_profile->currentIndex() < 2;
}

bool DvdWizardVob::isWide() const
{
    return (m_view.dvd_profile->currentIndex() == 1 || m_view.dvd_profile->currentIndex() == 3);
}

void DvdWizardVob::setProfile(const QString& profile)
{
    if (profile == "dv_pal") m_view.dvd_profile->setCurrentIndex(0);
    else if (profile == "dv_pal_wide") m_view.dvd_profile->setCurrentIndex(1);
    else if (profile == "dv_ntsc") m_view.dvd_profile->setCurrentIndex(2);
    else if (profile == "dv_ntsc_wide") m_view.dvd_profile->setCurrentIndex(3);
}

void DvdWizardVob::clear()
{
    m_view.vobs_list->clear();
}
