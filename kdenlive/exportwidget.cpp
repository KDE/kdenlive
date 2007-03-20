/***************************************************************************
                          exportwidget  -  description
                             -------------------
    begin                : Tue Nov 15 2005
    copyright            : (C) 2005 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qtabwidget.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qapplication.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qtabwidget.h>

#include <kurlrequester.h>
#include <kcombobox.h>
#include <kprogress.h>
#include <klocale.h>
#include <klistbox.h>
#include <klistview.h>
#include <krun.h>
#include <kservice.h>
#include <kuserprofile.h>
#include <kmimetype.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <klineedit.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <knotifyclient.h>

#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "exportwidget.h"
#include "editencoder_ui.h"
#include "editmetadata_ui.h"
#include "kdenlivesettings.h"
#include "krender.h"

#define PAL 1
#define NTSC 2
#define HDV1080PAL 10
#define HDV720PAL 11
#define CUSTOMFORMAT 20


exportWidget::exportWidget(Gui::KdenliveApp *app, Gui::KTimeLine *timeline, VIDEOFORMAT format, QWidget* parent, const char* name): exportBaseWidget_UI(parent,name), m_duration(0), m_exportProcess(NULL), m_convertProcess(NULL), m_app(app), m_timeline(timeline), m_tmpFile(NULL), m_format(format), m_emitSignal(false), m_meta_year(0), m_meta_track(0)

{
/*    m_node = -1;
    m_port = -1;
    m_guid = 0;
    m_avc = 0;*/
    setCaption(i18n("Render Timeline"));
    initEncoders();
    m_isRunning = false;
    fileExportFolder->setMode(KFile::Directory);
    fileExportFolder->fileDialog()->setOperationMode(KFileDialog::Saving);
    updateGuides();

    // custom templates not implemented yet
    //encoders->page(3)->setEnabled(false);
    
    initDvConnection();

    QStringList priority;
    priority<<i18n("Low")<<i18n("Normal")<<i18n("High");
    export_priority->insertStringList(priority);
    export_priority->setCurrentText(i18n("Normal"));

    slotLoadCustomEncoders();

    connect(exportButton,SIGNAL(clicked()),this,SLOT(startExport()));

    connect(guide_start, SIGNAL(activated(int)),this,SLOT(slotAdjustGuides(int)));
    connect(export_guide, SIGNAL(toggled(bool)), guide_box, SLOT(setEnabled(bool)));
    connect(export_guide, SIGNAL(toggled(bool)), this, SLOT(slotGuideZone(bool)));
    connect(export_selected, SIGNAL(toggled(bool)), this, SLOT(slotSelectedZone(bool)));

    connect(hq_encoders, SIGNAL(selectionChanged ()), this, SLOT(slotCheckSelection()));
    connect(med_encoders, SIGNAL(selectionChanged ()), this, SLOT(slotCheckSelection()));
    connect(audio_encoders, SIGNAL(selectionChanged ()), this, SLOT(slotCheckSelection()));
    connect(custom_encoders, SIGNAL(selectionChanged ()), this, SLOT(slotCheckSelection()));
    connect(encoders, SIGNAL(currentChanged ( QWidget * )), this, SLOT(slotCheckSelection()));
    connect(button_new, SIGNAL( clicked() ), this, SLOT( slotAddEncoder()));
    connect(button_edit, SIGNAL( clicked() ), this, SLOT( slotEditEncoder()));
    connect(button_delete, SIGNAL( clicked() ), this, SLOT( slotDeleteEncoder()));
    connect(button_metadata, SIGNAL( clicked() ), this, SLOT( slotEditMetaData()));
    connect(custom_encoders, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int ) ), this, SLOT( slotEditEncoder()));
}

exportWidget::~exportWidget()
{
    slotSaveCustomEncoders();
}

void exportWidget::setVideoFormat(VIDEOFORMAT format)
{
    m_format = format;
    initEncoders();
}

void exportWidget::setMetaData(QStringList metaValues)
{
	if (metaValues.count() != 7) {
	    //kdDebug()<<"///////  BAD METADATA INPUT ///////"<<endl;
	    return;
	}
	m_meta_author = metaValues[0];
	m_meta_title = metaValues[1];
	m_meta_comment = metaValues[2];
	m_meta_copyright = metaValues[3];
	m_meta_album = metaValues[4];
	m_meta_track = metaValues[5].toInt();
	m_meta_year = metaValues[6].toInt();
}

void exportWidget::resetValues()
{
	m_meta_author = QString::null;
	m_meta_title = QString::null;
	m_meta_comment = QString::null;
	m_meta_copyright = QString::null;
	m_meta_album = QString::null;
	m_meta_track = 0;
	m_meta_year = 0;
	updateGuides();
	fileExportName->setText("untitled.dv");
	fileExportFolder->setURL(KdenliveSettings::currentdefaultfolder());
}

const QStringList exportWidget::getMetaData()
{
	QStringList result;
	result<<m_meta_author;
	result<<m_meta_title;
	result<<m_meta_comment;
	result<<m_meta_copyright;
	result<<m_meta_album;
	result<<QString::number(m_meta_track);
	result<<QString::number(m_meta_year);
	return result;
}

void exportWidget::slotEditMetaData()
{
    editMetadata_UI *editMeta = new editMetadata_UI(this);
    editMeta->meta_author->setText(m_meta_author);
    editMeta->meta_title->setText(m_meta_title);
    editMeta->meta_comment->setText(m_meta_comment);
    editMeta->meta_copyright->setText(m_meta_copyright);
    editMeta->meta_album->setText(m_meta_album);
    editMeta->meta_track->setValue(m_meta_track);
    editMeta->meta_year->setText(QString::number(m_meta_year));
    if (editMeta->exec() == QDialog::Accepted ) {
	m_meta_author = editMeta->meta_author->text();
	m_meta_title = editMeta->meta_title->text();
	m_meta_comment = editMeta->meta_comment->text();
	m_meta_album = editMeta->meta_album->text();
	m_meta_copyright = editMeta->meta_copyright->text();
	m_meta_track = editMeta->meta_track->value();
	m_meta_year = editMeta->meta_year->text().toInt();
	QStringList newMetadata = getMetaData();
	emit metadataChanged( newMetadata );
    }
    delete editMeta;
}


QStringList exportWidget::metadataString()
{
    QStringList result;
    char *tmp;
    
    if (!m_meta_author.isEmpty()) {
	tmp = KRender::decodedString(QString( "meta.attr.author.markup=" + m_meta_author));
	result << tmp;
    	delete tmp;
    }
    
    if (!m_meta_title.isEmpty()) {
	tmp = KRender::decodedString(QString( "meta.attr.title.markup=" + m_meta_title));
	result << tmp;
	delete tmp;
    }
    if (!m_meta_comment.isEmpty()) {
	tmp = KRender::decodedString(QString( "meta.attr.comment.markup=" + m_meta_comment));
	result << tmp;
	delete tmp;
    }
    if (!m_meta_copyright.isEmpty()) {
	tmp = KRender::decodedString(QString( "meta.attr.copyright.markup=" + m_meta_copyright));
	result << tmp;
	delete tmp;
    }
    if (!m_meta_album.isEmpty()) {
	tmp = KRender::decodedString(QString( "meta.attr.album.markup=" + m_meta_album));
	result << tmp;
	delete tmp;
    }
    if (m_meta_year != 0) result << QString( "meta.attr.year.markup=" + QString::number(m_meta_year));
    if (m_meta_track != 0) result << QString( "meta.attr.track.markup=" + QString::number(m_meta_track));
    return result;
}

void exportWidget::slotGuideZone(bool isOn)
{
    if (isOn) export_selected->setChecked(false);
}

void exportWidget::slotSelectedZone(bool isOn)
{
    if (isOn) export_guide->setChecked(false);
}

void exportWidget::slotSaveCustomEncoders()
{
    QString txt;
    QStringList::Iterator it;
    for ( it = CustomEncoders.begin(); it != CustomEncoders.end(); ++it ) {
	txt+= (*it) + "\n";
    }

    QString exportFile = locateLocal("data", "kdenlive/profiles/custom.profile");
    QFile file(exportFile);
    file.open(IO_WriteOnly);
    QTextStream stream( &file );
    stream.setEncoding (QTextStream::UnicodeUTF8);
    stream << txt;
    file.close();
}

void exportWidget::slotLoadCustomEncoders()
{
    QString exportFile = locateLocal("data", "kdenlive/profiles/custom.profile");

    QFile myFile(exportFile);
    if (myFile.open(IO_ReadOnly)) {
	QTextStream stream( &myFile );
	stream.setEncoding (QTextStream::UnicodeUTF8);
	QString line = stream.readLine();
	while (!line.isEmpty()) {
	    CustomEncoders<<line;
	    (void) new QListViewItem(custom_encoders, line.section(":", 2, 2));
	    line = stream.readLine();
	}
	myFile.close();
    }
}


void exportWidget::slotEditEncoder()
{
    if (!custom_encoders->currentItem()) return;
    EditEncoder_UI dlg(this, "edit_encode");
    QString enc_name = custom_encoders->currentItem()->text(0);
    dlg.encoder_name->setText(enc_name);
    QString param;
    QString ext;
    QStringList::Iterator it;
    for ( it = CustomEncoders.begin(); it != CustomEncoders.end(); ++it ) {
	if ((*it).section(":", 2, 2) == enc_name) {
		param = (*it).section(":", 9);
		ext = (*it).section(":", 8, 8);
		break;
	}
    }
    dlg.encoder_param->setText(param);
    dlg.encoder_ext->setText(ext);
    if (dlg.exec() == QDialog::Accepted) {
	CustomEncoders.erase(it);
	delete custom_encoders->currentItem();
	QListViewItem *item = new QListViewItem(custom_encoders, dlg.encoder_name->text());
	custom_encoders->setCurrentItem(item);
	CustomEncoders<<"0:Custom:" + dlg.encoder_name->text() + "::::avformat::" + dlg.encoder_ext->text() + ":" + dlg.encoder_param->text().simplifyWhiteSpace();
    }
}

void exportWidget::slotEditEncoder(QString name, QString param, QString ext)
{
    bool found = false;
    EditEncoder_UI dlg(this, "edit_encode");
    dlg.encoder_name->setText(name);
    dlg.encoder_param->setText(param);
    dlg.encoder_ext->setText(ext);
    if (dlg.exec() == QDialog::Accepted) {
	for ( QStringList::Iterator it = CustomEncoders.begin(); it != CustomEncoders.end(); ++it ) {
	    if ((*it).section(":", 2, 2) == dlg.encoder_name->text()) {
		KMessageBox::sorry(this, i18n("An encoder named %1 already exists, please choose another name.").arg(dlg.encoder_name->text()));
	    	slotEditEncoder(dlg.encoder_name->text(), dlg.encoder_param->text(), dlg.encoder_ext->text());
	    	found = true;
		break;
	    }
	}
	if (!found) {
	    (void) new QListViewItem(custom_encoders, dlg.encoder_name->text());
	    CustomEncoders<<"0:Custom:" + dlg.encoder_name->text() + "::::avformat::" + dlg.encoder_ext->text() + ":" + dlg.encoder_param->text().simplifyWhiteSpace();
	}
    }
}

void exportWidget::slotDeleteEncoder()
{
    if (!custom_encoders->currentItem()) return;
    QString enc_name = custom_encoders->currentItem()->text(0);
    QStringList::Iterator it;
    for ( it = CustomEncoders.begin(); it != CustomEncoders.end(); ++it ) {
	if ((*it).section(":", 2, 2) == enc_name) {
		break;
	}
    }
    if (it == CustomEncoders.end()) {
	kdDebug()<<"ITEM Not found in list"<<endl;
	return;
    }
    CustomEncoders.erase(it);
    delete custom_encoders->currentItem();
}

void exportWidget::slotAddEncoder()
{
    bool found = false;
    EditEncoder_UI dlg(this, "new_encode");
    if (dlg.exec() == QDialog::Accepted) {
	for ( QStringList::Iterator it = CustomEncoders.begin(); it != CustomEncoders.end(); ++it ) {
	    if ((*it).section(":", 2, 2) == dlg.encoder_name->text()) {
		KMessageBox::sorry(this, i18n("An encoder named %1 already exists, please choose another name.").arg(dlg.encoder_name->text()));
	    	slotEditEncoder(dlg.encoder_name->text(), dlg.encoder_param->text(), dlg.encoder_ext->text());
	    	found = true;
		break;
	    }
	}
	if (!found && !dlg.encoder_name->text().stripWhiteSpace().simplifyWhiteSpace().isEmpty()) {
	    (void) new QListViewItem(custom_encoders, dlg.encoder_name->text());
	    CustomEncoders<<"0:Custom:" + dlg.encoder_name->text() + "::::avformat::" + dlg.encoder_ext->text() + ":" + dlg.encoder_param->text().simplifyWhiteSpace();
	}
    }
}

QString exportWidget::slotCommandForItem(QStringList list, QListViewItem *item)
{
    QString itemText;
    QString itemParent;
    QString itemParent2;

    itemText = item->text(0);
    if (item->parent()) {
	itemParent = item->parent()->text(0);
    	if (item->parent()->parent()) itemParent2 = item->parent()->parent()->text(0);
    }
    if (!itemParent2.isEmpty()) return slotEncoderCommand(list, itemParent2, itemParent, itemText);
    else if (!itemParent.isEmpty()) return slotEncoderCommand(list, itemParent, itemText);
    else return slotEncoderCommand(list, itemText);
}

QString exportWidget::slotEncoderCommand(QStringList list, QString arg1, QString arg2, QString arg3)
{
    for ( QStringList::Iterator it = list.begin(); it != list.end(); ++it ) {
	if ((*it).section(":", 2, 2) == arg1) {
	    if (arg2.isEmpty()) {
		if ((*it).section(":", 7, 7).isEmpty()) {
			return (*it);
			break;
		}
		else if (((*it).section(":", 7, 7) == "PAL" && (m_format == PAL_VIDEO || m_format == PAL_WIDE)) || ((*it).section(":", 7, 7) == "NTSC" && (m_format == NTSC_VIDEO || m_format == NTSC_WIDE))) {
			return (*it);
			break;
		}
	    }
	    if ((*it).section(":", 3, 3) == arg2) {
	        if (arg3.isEmpty()) {
		    return (*it);
		    break;
	    	}
	        if ((*it).section(":", 4, 4) == arg3) {
		    return (*it);
		    break;
	        }
	    }
	}
    }
    return QString::null;
}


void exportWidget::slotCheckSelection()
{
    QString currentName=fileExportName->text();
    if (currentName.isEmpty()) currentName = "untitled.dv";
    int i = currentName.findRev(".");
    if (i!=-1) currentName = currentName.left(i);


    if (encoders->currentPageIndex() == 0) {
	if (hq_encoders->childCount() == 0) {
	    exportButton->setEnabled(false);
	    return;
	}
	if (hq_encoders->currentItem()->childCount() > 0) {
	    exportButton->setEnabled(false);
	    encoder_command->setText(QString::null);
	}
	else {
	    exportButton->setEnabled(true);
	    QString encoderCommand = slotCommandForItem(HQEncoders, hq_encoders->currentItem());
    	    fileExportName->setText(currentName+"." + encoderCommand.section(":", 8, 8));
	    encoder_command->setText(encoderCommand.section(":",9));
	    encoder_norm = encoderCommand.section(":",7,7);
	}
    }
    else if (encoders->currentPageIndex() == 1) {
	if (med_encoders->childCount() == 0) {
	    exportButton->setEnabled(false);
	    return;
	}
	if (med_encoders->currentItem()->childCount() > 0) {
	    exportButton->setEnabled(false);
	    encoder_command->setText(QString::null);
	}
	else {
	    exportButton->setEnabled(true);
	    QString encoderCommand = slotCommandForItem(MedEncoders, med_encoders->currentItem());
    	    fileExportName->setText(currentName+"." + encoderCommand.section(":", 8, 8));
	    encoder_command->setText(encoderCommand.section(":",9));
	    encoder_norm = encoderCommand.section(":",7,7);
	}
    }
    else if (encoders->currentPageIndex() == 2) {
	if (audio_encoders->childCount() == 0) {
	    exportButton->setEnabled(false);
	    return;
	}
	if (audio_encoders->currentItem()->childCount() > 0) {
	    exportButton->setEnabled(false);
	    encoder_command->setText(QString::null);
	}
	else {
	    exportButton->setEnabled(true);
	    QString encoderCommand = slotCommandForItem(AudioEncoders, audio_encoders->currentItem());
    	    fileExportName->setText(currentName+"." + encoderCommand.section(":", 8, 8));
	    encoder_command->setText(encoderCommand.section(":",9));
	    encoder_norm = encoderCommand.section(":",7,7);

	}
    }
    else if (encoders->currentPageIndex() == 3) {
	if (custom_encoders->childCount() == 0) {
	    exportButton->setEnabled(false);
	    return;
	}
	if (custom_encoders->currentItem()->childCount() > 0) {
	    exportButton->setEnabled(false);
	    encoder_command->setText(QString::null);
	}
	else {
	    exportButton->setEnabled(true);
	    QString encoderCommand = slotCommandForItem(CustomEncoders, custom_encoders->currentItem());
    	    fileExportName->setText(currentName+"." + encoderCommand.section(":", 8, 8));
	    encoder_command->setText(encoderCommand.section(":",9));
	}
    }
}

void exportWidget::updateGuides()
{
    guide_start->clear();
    guide_end->clear();
    m_guidesList = m_timeline->timelineRulerComments();
    bool enable = m_guidesList.count() > 1;
    guide_start->setEnabled(enable);
    guide_end->setEnabled(enable);
    export_guide->setEnabled(enable);
    if (!enable) return;
    QStringList startGuides = m_guidesList;
    startGuides.pop_back();
    guide_start->insertStringList(startGuides);
    QStringList endGuides = m_guidesList;
    endGuides.pop_front();
    guide_end->insertStringList(endGuides);
}

void exportWidget::slotAdjustGuides(int ix)
{
    QStringList endGuides = m_guidesList;
    while (ix >= 0) {
        endGuides.pop_front();
	ix--;
    }
    guide_end->clear();
    guide_end->insertStringList(endGuides);
}

void exportWidget::initDvConnection()
{

}


void exportWidget::initEncoders()
{
    fileExportName->setText("untitled.dv");
    fileExportFolder->setURL(KdenliveSettings::currentdefaultfolder());

    // Find all profiles and add them to the list

    QString exportFile = locate("data", "kdenlive/profiles/exports.profile");
    QFile file(exportFile);
    QString line;
    hq_encoders->clear();
    med_encoders->clear();
    audio_encoders->clear();
    HQEncoders.clear();
    MedEncoders.clear();
    AudioEncoders.clear();

    if ( file.open( IO_ReadOnly ) ) {
        QTextStream stream( &file );
        while ( !stream.atEnd() ) {
            line = stream.readLine(); // line of text excluding '\n'
	    if (!line.startsWith("#")) {
		if (line.section(":",1,1) == "HQ") {
		    if ((line.section(":",7,7) == "PAL" && KdenliveSettings::defaultheight() == 576) || (line.section(":",7,7) == "NTSC" && KdenliveSettings::defaultheight() == 480)) {
			QString name = line.section(":",2,2);
			HQEncoders<<line;
			QListViewItem *item =  hq_encoders->findItem(name, 0);
			if (!item) item = new KListViewItem(hq_encoders, name);
			/*QListViewItem *child = item->firstChild();
			if (!child) child = new KListViewItem(item, line.section(":",3,3));
			else {
			    bool found = false;
			    while (child) {
				if (child->text(0) == line.section(":",3,3)) {
					found = true;
					break;
				}
				child = child->nextSibling();
			    }
			    if (!found) child = new KListViewItem(item, line.section(":",3,3));
			}		
			if (!line.section(":",4,4).isEmpty()) (void) new KListViewItem(child, line.section(":",4,4));
			*/
		    }
		}
		else if (line.section(":",1,1) == "MED") {
			MedEncoders<<line;
			QString name = line.section(":",2,2);
			QListViewItem *item =  med_encoders->findItem(name, 0);
			if (!item) item = new KListViewItem(med_encoders, name);
			QListViewItem *child = item->firstChild();
			if (!child) child = new KListViewItem(item, line.section(":",3,3));
			else {
			    bool found = false;
			    while (child) {
				if (child->text(0) == line.section(":",3,3)) {
					found = true;
					break;
				}
				child = child->nextSibling();
			    }
			    if (!found) child = new KListViewItem(item, line.section(":",3,3));
			}
			if (!line.section(":",4,4).isEmpty()) (void) new KListViewItem(child, line.section(":",4,4));
		}
		else if (line.section(":",1,1) == "AUDIO") {
			AudioEncoders<<line;
			QString name = line.section(":",2,2);
			QListViewItem *item =  audio_encoders->findItem(name, 0);
			if (!item) item = new KListViewItem(audio_encoders, name);
			QListViewItem *child = item->firstChild();
			if (!child) child = new KListViewItem(item, line.section(":",3,3));
			else {
			    bool found = false;
			    while (child) {
				if (child->text(0) == line.section(":",3,3)) {
					found = true;
					break;
				}
				child = child->nextSibling();
			    }
			    if (!found) child = new KListViewItem(item, line.section(":",3,3));
			}
			if (!line.section(":",4,4).isEmpty()) (void) new KListViewItem(child, line.section(":",4,4));
		}

	    }
	}
    }

}


void exportWidget::stopExport()
{
    if (m_exportProcess) {
	m_exportProcess->kill();
    }
    if (m_convertProcess) {
	m_convertProcess->kill();
    }
    //emit stopTimeLineExport();
}

void exportWidget::startExport()
{
        if (fileExportName->text().isEmpty()) {
            KMessageBox::sorry(this, i18n("Please enter a file name"));
            return;
        }
        if (m_isRunning) {
            stopExport();
            return;
        }
        
        if (KIO::NetAccess::exists(KURL(fileExportFolder->url()+"/"+fileExportName->text()), false, this))
            if (KMessageBox::questionYesNo(this, i18n("File already exists.\nDo you want to overwrite it ?")) ==  KMessageBox::No) return;
        
        if (export_selected->isChecked()) {
            startExportTime = m_timeline->inpointPosition();
            endExportTime = m_timeline->outpointPosition();
        }
        else if (export_guide->isChecked()){
	    startExportTime = m_timeline->guideTime(guide_start->currentItem ());
            endExportTime = m_timeline->guideTime(guide_end->currentItem () + guide_start->currentItem () + 1);
	} else {
            startExportTime = GenTime(0);
            endExportTime = m_timeline->projectLength();
        }
        m_duration = endExportTime - startExportTime;
	QString paramLine;
	switch (encoders->currentPageIndex()) {
	case 0:
		paramLine = slotCommandForItem(HQEncoders, hq_encoders->currentItem());
		break;
	case 1: 
		paramLine = slotCommandForItem(MedEncoders, med_encoders->currentItem());
		break;
	case 2: 
		paramLine = slotCommandForItem(AudioEncoders, audio_encoders->currentItem());
		break;
	case 3:
		paramLine = slotCommandForItem(CustomEncoders, custom_encoders->currentItem());
		break;
	}
	paramLine = paramLine.stripWhiteSpace();
	paramLine = paramLine.simplifyWhiteSpace();
	m_createdFile = fileExportFolder->url()+"/"+fileExportName->text();
	if (paramLine.section(":", 6, 6) == "libdv") doExport(fileExportFolder->url()+"/"+fileExportName->text(), 0, QStringList(), true);
	else {
	    double ratio = getCurrentAspect();
	    doExport(fileExportFolder->url()+"/"+fileExportName->text(), ratio, QStringList::split(" ", paramLine.section(":", 9)));
	} 

	// Hide dialog when export starts
	hide();
}

double exportWidget::getCurrentAspect()
{
	QString size;
	int width, height;
	double aspect = 0;
	switch (encoders->currentPageIndex()) {
	case 0:
		break;
	case 1: 
		size = med_encoders->currentItem()->parent()->text(0);
		width = size.section("x", 0, 0).toInt();
		height = size.section("x", 1, 1).toInt();
		aspect = 4.0/3.0*height/width;
		break;
	default: 
		break;
	}
	return aspect;
}

void exportWidget::renderSelectedZone(const QString &url, bool audioOnly)
{
    if (m_isRunning) {
	if (KMessageBox::questionYesNo(this, i18n("There is another file render currently running, cancel it ?")) != KMessageBox::Yes) return;
        stopExport();
    }
    m_createdFile = url;
    startExportTime = m_timeline->inpointPosition();
    endExportTime = m_timeline->outpointPosition();
    m_duration = endExportTime - startExportTime;
    doExport(url, 0, QStringList(), true, audioOnly);
    m_emitSignal = true;
}

void exportWidget::renderSelectedClipAudio(const QString &source, const QString &dest)
{
    if (m_isRunning) {
	if (KMessageBox::questionYesNo(this, i18n("There is another file render currently running, cancel it ?")) != KMessageBox::Yes) return;
        stopExport();
    }
    m_createdFile = dest;
    doAudioExport(source, dest);
    m_emitSignal = true;
}

void exportWidget::generateDvdFile(QString file, GenTime start, GenTime end, VIDEOFORMAT format)
{
    QStringList encoderParams;
    m_isRunning = true;
    startExportTime = start;
    endExportTime = end;
    m_duration = endExportTime - startExportTime;
    if (m_tmpFile) delete m_tmpFile;
    m_tmpFile = new KTempFile( QString::null, ".westley");
    m_progress = -1;
    if (m_exportProcess) {
    	m_exportProcess->kill();
    	delete m_exportProcess;
    }
    QTextStream stream( m_tmpFile->file() );
    stream << m_app->getDocument()->projectClip().generateSceneList(true, true).toString() << "\n";
    m_tmpFile->file()->close();
    m_exportProcess = new KProcess;
    if (format == PAL_VIDEO || format == PAL_WIDE) {
	m_exportProcess->setEnvironment("MLT_NORMALISATION", "PAL");
	encoderParams = QStringList::split(" ",slotEncoderCommand(HQEncoders, "DVD", "PAL").section(":",9));
    }
    else if (format == NTSC_VIDEO || format == NTSC_WIDE) {
	m_exportProcess->setEnvironment("MLT_NORMALISATION", "NTSC");
	encoderParams = QStringList::split(" ",slotEncoderCommand(HQEncoders, "DVD", "NTSC").section(":",9));
    }
    //kdDebug()<<" + + DVD EXPORT, PARAMS: "<<encoderParams<<endl;
    *m_exportProcess << "kdenlive_renderer";
    *m_exportProcess << m_tmpFile->name();
    *m_exportProcess << "real_time=0";
    *m_exportProcess << "resize=hyper";
    //*m_exportProcess << "progressive=1";
    *m_exportProcess << QString("in=%1").arg(start.frames(KdenliveSettings::defaultfps()));
    *m_exportProcess << QString("out=%1").arg(end.frames(KdenliveSettings::defaultfps()));
    *m_exportProcess << "-consumer";
    *m_exportProcess << QString("avformat:%1").arg(file);
    *m_exportProcess << "real_time=0";
    *m_exportProcess << "stats_on=1";
    // workaround until MLT's default qscale value is fixed
    *m_exportProcess << "qscale=1";
    if (!KdenliveSettings::videoprofile().isEmpty()) 
	*m_exportProcess<<"profile=" + KdenliveSettings::videoprofile();
    *m_exportProcess << encoderParams;

    connect(m_exportProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endDvdExport(KProcess *)));
    connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));
    m_exportProcess->start(KProcess::NotifyOnExit, KProcess::AllOutput);

}

void exportWidget::doExport(QString file, double ratio, QStringList params, bool isDv, bool audioOnly)
{
    if (m_tmpFile) delete m_tmpFile;
    m_tmpFile = new KTempFile( QString::null, ".westley");
    m_progress = -1;
    if (m_exportProcess) {
    	m_exportProcess->kill();
    	delete m_exportProcess;
    }
    m_isRunning = true;
    exportButton->setText(i18n("Stop"));
    kdDebug()<<"++++++  PREPARE TO WRITE TO: "<<m_tmpFile->name()<<endl;
    QTextStream stream( m_tmpFile->file() );
    stream << m_app->getDocument()->projectClip().generateSceneList(true, true).toString() << "\n";
    m_tmpFile->file()->close();
    m_exportProcess = new KProcess;

    if (!encoder_norm.isEmpty()) m_exportProcess->setEnvironment("MLT_NORMALISATION", encoder_norm);
    else if (m_format == PAL_VIDEO || m_format == PAL_WIDE) m_exportProcess->setEnvironment("MLT_NORMALISATION", "PAL");
    else if (m_format == NTSC_VIDEO || m_format == NTSC_WIDE) m_exportProcess->setEnvironment("MLT_NORMALISATION", "NTSC");
    *m_exportProcess << "kdenlive_renderer";

    *m_exportProcess << m_tmpFile->name();
    *m_exportProcess << "real_time=0";
    *m_exportProcess << "resize=hyper";
//    *m_exportProcess << "progressive=1";
    *m_exportProcess << QString("in=%1").arg(startExportTime.frames(KdenliveSettings::defaultfps()));
    *m_exportProcess << QString("out=%1").arg(endExportTime.frames(KdenliveSettings::defaultfps()));
    *m_exportProcess << "-consumer";
    if (isDv) {
	*m_exportProcess << QString("libdv:%1").arg(file);
	*m_exportProcess << "terminate_on_pause=1";
    }
    else
    *m_exportProcess << QString("avformat:%1").arg(file);
    if (audioOnly) *m_exportProcess <<"format=wav"<<"frequency=48000";
    else { 
	*m_exportProcess << params;
        if (ratio != 0) *m_exportProcess << QString("aspect_ratio=") + QString::number(ratio);
    }
    if (addMetadata->isChecked()) *m_exportProcess << metadataString();
    *m_exportProcess << "real_time=0";
    *m_exportProcess << "stats_on=1";
    // workaround until MLT's default qscale value is fixed
    *m_exportProcess << "qscale=1";
    if (isDv && !KdenliveSettings::videoprofile().isEmpty()) 
	*m_exportProcess<<"profile=" + KdenliveSettings::videoprofile();
    connect(m_exportProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endExport(KProcess *)));
    connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));

    switch (export_priority->currentItem()) {
	case 0:
	    m_exportProcess->setPriority(15);
	    break;
	case 1:
	    m_exportProcess->setPriority(0);
	    break;
	case 2:
	    m_exportProcess->setPriority(-15);
	    break;
    }
    m_exportProcess->start(KProcess::NotifyOnExit, KProcess::Stderr);
}

void exportWidget::doAudioExport(QString src, QString dest)
{
    if (m_tmpFile) delete m_tmpFile;
    m_tmpFile = new KTempFile( QString::null, ".westley");
    QTextStream stream( m_tmpFile->file() );
    stream << src << "\n";
    m_tmpFile->file()->close();

    m_progress = -1;
    if (m_exportProcess) {
    	m_exportProcess->kill();
    	delete m_exportProcess;
    }

    m_isRunning = true;
    exportButton->setText(i18n("Stop"));
    m_exportProcess = new KProcess;

    if (!encoder_norm.isEmpty()) m_exportProcess->setEnvironment("MLT_NORMALISATION", encoder_norm);
    else if (m_format == PAL_VIDEO || m_format == NTSC_WIDE) m_exportProcess->setEnvironment("MLT_NORMALISATION", "PAL");
    else if (m_format == NTSC_VIDEO || m_format == NTSC_WIDE) m_exportProcess->setEnvironment("MLT_NORMALISATION", "NTSC");
    *m_exportProcess << "kdenlive_renderer";

    *m_exportProcess << m_tmpFile->name();
    *m_exportProcess << "real_time=0";
    *m_exportProcess << "-consumer";
    *m_exportProcess << QString("avformat:%1").arg(dest);
    *m_exportProcess << "format=wav" << "frequency=48000";
    *m_exportProcess << "real_time=0";
    *m_exportProcess << "stats_on=1";
    // workaround until MLT's default qscale value is fixed
    *m_exportProcess << "qscale=1";
    connect(m_exportProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endExport(KProcess *)));
    connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));

    switch (export_priority->currentItem()) {
	case 0:
	    m_exportProcess->setPriority(15);
	    break;
	case 1:
	    m_exportProcess->setPriority(0);
	    break;
	case 2:
	    m_exportProcess->setPriority(-15);
	    break;
    }
    m_exportProcess->start(KProcess::NotifyOnExit, KProcess::Stderr);
}

void exportWidget::receivedStderr(KProcess *, char *buffer, int len)
{
	QCString res(buffer, len);
	QString result = res;
	result = result.simplifyWhiteSpace();
	result = result.section(" ", -1);
	int progress = (int) (100.0 * result.toInt() / m_duration.frames(KdenliveSettings::defaultfps()));
	if (progress > m_progress) {
		m_progress = progress;
		QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(progress, 10007));
	}
}

void exportWidget::receivedConvertStderr(KProcess *, char *buffer, int )
{
	QString result = QString(buffer);
	result = result.simplifyWhiteSpace();
	result = result.section(" ", 0, 0);
	int hours = result.section(":", 0, 0).toInt();
	int minutes = result.section(":", 1, 1).toInt();
	int seconds = result.section(":", 2, 2).section(".", 0, 0).toInt();
	int milliseconds = result.section(":", 2, 2).section(".", 1, 1).toInt();
	int defaultfps = (int) KdenliveSettings::defaultfps();
	int progress = hours * 3600 * defaultfps + minutes * 60 * defaultfps + seconds * defaultfps + milliseconds * defaultfps / 100;
	//kdDebug()<<"++ THEORA: "<<result<<", FRAMES: "<<progress<<", DURATION: "<<m_duration.frames(KdenliveSettings::defaultfps())<<endl;

	if (progress > m_progress) {
		m_progress = progress;
		QApplication::postEvent(qApp->mainWidget(), new ProgressEvent((int) (100.0 * progress / m_duration.frames(KdenliveSettings::defaultfps())), 10007));
	}
}

void exportWidget::endExport(KProcess *)
{
    bool finishedOK = true;
    bool twoPassEncoding = false;
    if (m_tmpFile) {
        m_tmpFile->unlink();
        delete m_tmpFile;
        m_tmpFile = 0;
    }
    //if (EncodersMap[encoders->currentText()] == "theora") twoPassEncoding = true; 

    if (!m_exportProcess->normalExit()) {
	KNotifyClient::event(winId(), "RenderError", i18n("The export terminated unexpectedly.\nOutput file will probably be corrupted..."));
	finishedOK = false;
    }
    else if (!twoPassEncoding) {
	QPixmap px(KGlobal::iconLoader()->loadIcon("kdenlive", KIcon::Toolbar));
	KNotifyClient::event(winId(), "RenderOk", i18n("Export of %1 is finished").arg(m_createdFile));
	
    }
    if (m_emitSignal) emit addFileToProject(m_createdFile);
    delete m_exportProcess;
    m_exportProcess = 0;

    /*if (EncodersMap[encoders->currentText()] == "theora") {
	QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(0, 10007));
	//exportFileToTheora(KURL(fileExportFolder->url()+"/"+fileExportName->text() + ".dv").path(), vquality->currentText().toInt(), aquality->currentText().toInt(), videoSize->currentText());
    }*/
	exportButton->setText(i18n("Export"));
    	m_isRunning = false;
	QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(-1, 10007));
    	//processProgress->setProgress(0);
    	if (autoPlay->isChecked() && finishedOK) {
	        (void) new KRun(KURL(m_createdFile));
    	}
}


void exportWidget::endDvdExport(KProcess *)
{
    kdDebug()<<"* * * * * * * * *DVD FINISHED"<<endl;
    bool finishedOK = true;
    m_tmpFile->unlink();
    delete m_tmpFile;
    m_tmpFile = 0;

    if (!m_exportProcess->normalExit()) {
	//KMessageBox::sorry(this, i18n("The export terminated unexpectedly.\nOutput file will probably be corrupted..."));
	emit dvdExportOver(false);
    }
    else emit dvdExportOver(true);
    delete m_exportProcess;
    m_exportProcess = 0;
    m_isRunning = false;
    QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(-1, 10007));
}


void exportWidget::endConvert(KProcess *)
{
    bool finishedOK = true;
    if (!m_convertProcess->normalExit()) {
	KMessageBox::sorry(this, i18n("The conversion terminated unexpectedly.\nOutput file will probably be corrupted..."));
	finishedOK = false;
    }
    delete m_convertProcess;
    m_convertProcess = 0;
    exportButton->setText(i18n("Export"));
    KIO::NetAccess::del(KURL(fileExportFolder->url()+"/"+fileExportName->text() + ".dv"), this);
    m_isRunning = false;
    QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(-1, 10007));
    //processProgress->setProgress(0);
    if (autoPlay->isChecked() && finishedOK) {
	(void) new KRun(KURL(fileExportFolder->url()+"/"+fileExportName->text()));
    }

}

bool exportWidget::isRunning()
{
    return m_isRunning;
}

void exportWidget::endExport()
{
    exportButton->setText(i18n("Export"));
    m_isRunning = false;
/*    if (encoders->currentText() == "theora") {
//	exportFileToTheora(KURL(fileExportFolder->url()+"/"+fileExportName->text() + ".dv").path(), vquality->currentText().toInt(), aquality->currentText().toInt(), videoSize->currentText());
    }
    else */{
    	//processProgress->setProgress(0);
    	if (autoPlay->isChecked ()) {
	        //KRun *run=new KRun(KURL(fileExportFolder->url()+"/"+fileExportName->text()));
    	}
    }
}

void exportWidget::exportFileToTheora(QString srcFileName, int video, int audio, QString size)
{
    if (m_convertProcess) {
    	m_convertProcess->kill();
    	delete m_convertProcess;
    }
    m_progress = -1;
    QString dstFileName = srcFileName.left(srcFileName.findRev("."));

    m_convertProcess = new KProcess;
    *m_convertProcess << "ffmpeg2theora";
    *m_convertProcess << srcFileName;
    *m_convertProcess << "-a";
    *m_convertProcess << QString::number(audio);
    *m_convertProcess << "-v";
    *m_convertProcess << QString::number(video);
    *m_convertProcess << "-f";
    *m_convertProcess << "dv";
    *m_convertProcess << "-x";
    *m_convertProcess << size.section("x", 0, 0);
    *m_convertProcess << "-y";
    *m_convertProcess << size.section("x", 1, 1);
    *m_convertProcess << "-o";
    *m_convertProcess << dstFileName;

    connect(m_convertProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endConvert(KProcess *)));
    connect(m_convertProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedConvertStderr(KProcess *, char *, int)));
    m_convertProcess->start(KProcess::NotifyOnExit, KProcess::AllOutput);
}

