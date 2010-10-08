/***************************************************************************
                          stopmotion.cpp  -  description
                             -------------------
    begin                : Feb 28 2008
    copyright            : (C) 2010 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "stopmotion.h"
#include "../blackmagic/devices.h"
#include "../slideshowclip.h"
#include "kdenlivesettings.h"


#include <KDebug>
#include <KGlobalSettings>
#include <KFileDialog>
#include <KStandardDirs>
#include <KMessageBox>
#include <kdeversion.h>
#include <KNotification>

#include <QComboBox>
#include <QVBoxLayout>
#include <QTimer>
#include <QAction>

StopmotionWidget::StopmotionWidget(KUrl projectFolder, QWidget *parent) :
	m_projectFolder(projectFolder)
        , QDialog(parent)
        , Ui::Stopmotion_UI()
	, m_sequenceFrame(0)
{
    setupUi(this);
    setWindowTitle(i18n("Stop Motion Capture"));
    setFont(KGlobalSettings::toolBarFont());

    live_button->setIcon(KIcon("camera-photo"));
    frameoverlay_button->setIcon(KIcon("edit-paste"));
    m_captureAction = new QAction(KIcon("media-record"), i18n("Capture frame"), this);
    m_captureAction->setShortcut(QKeySequence(Qt::Key_Space));
    connect(m_captureAction, SIGNAL(triggered()), this, SLOT(slotCaptureFrame()));
    capture_button->setDefaultAction(m_captureAction);
    
    preview_button->setIcon(KIcon("media-playback-start"));
    removelast_button->setIcon(KIcon("edit-delete"));

    capture_button->setEnabled(false);
    frameoverlay_button->setEnabled(false);
    removelast_button->setEnabled(false);

#if KDE_IS_VERSION(4,4,0)
    sequence_name->setClickMessage(i18n("Enter sequence name..."));
#endif

    connect(sequence_name, SIGNAL(textChanged(const QString &)), this, SLOT(sequenceNameChanged(const QString &)));
    BMInterface::getBlackMagicDeviceList(capture_device, NULL);
    QVBoxLayout *lay = new QVBoxLayout;
    m_bmCapture = new CaptureHandler(lay);
    video_preview->setLayout(lay);
    live_button->setChecked(false);
    frameoverlay_button->setChecked(false);
    button_addsequence->setEnabled(false);
    connect(live_button, SIGNAL(clicked(bool)), this, SLOT(slotLive(bool)));
    connect(frameoverlay_button, SIGNAL(clicked(bool)), this, SLOT(slotShowOverlay(bool)));
    connect(frame_number, SIGNAL(valueChanged(int)), this, SLOT(slotShowFrame(int)));
    connect(button_addsequence, SIGNAL(clicked(bool)), this, SLOT(slotAddSequence()));
}

StopmotionWidget::~StopmotionWidget()
{
    m_bmCapture->stopPreview();
}

void StopmotionWidget::slotLive(bool isOn)
{
    if (isOn) {
	m_bmCapture->startPreview(KdenliveSettings::hdmicapturedevice(), KdenliveSettings::hdmicapturemode());
	capture_button->setEnabled(sequence_name->text().isEmpty() == false);
    }
    else {
	m_bmCapture->stopPreview();
	capture_button->setEnabled(false);
    }
}

void StopmotionWidget::slotShowOverlay(bool isOn)
{
    if (isOn) {
	if (live_button->isChecked() && m_sequenceFrame > 0) {
	    slotUpdateOverlay();
	}
    }
    else {
	m_bmCapture->hideOverlay();
    }
}

void StopmotionWidget::slotUpdateOverlay()
{
    QString path = getPathForFrame(m_sequenceFrame - 1);
    if (!QFile::exists(path)) return;
    QImage img(path);
    if (img.isNull()) {
	QTimer::singleShot(1000, this, SLOT(slotUpdateOverlay()));
	return;
    }
    m_bmCapture->showOverlay(img);
}

void StopmotionWidget::sequenceNameChanged(const QString &name)
{
    if (name.isEmpty()) {
	button_addsequence->setEnabled(false);
	capture_button->setEnabled(false);
	frame_number->blockSignals(true);
	frame_number->setValue(m_sequenceFrame);
	frame_number->blockSignals(false);
	frameoverlay_button->setEnabled(false);
	removelast_button->setEnabled(false);
    }
    else {
	// Check if we are editing an existing sequence
	int count = 0;
	QString pattern = SlideshowClip::selectedPath(getPathForFrame(0, sequence_name->text()), false, QString(), &count);
	m_sequenceFrame = count;
	if (count > 0) {
	    m_sequenceName = sequence_name->text();
	    button_addsequence->setEnabled(true);
	    frameoverlay_button->setEnabled(true);
	}
	else {
	    button_addsequence->setEnabled(false);
	    frameoverlay_button->setEnabled(false);
	}
	frame_number->setRange(0, m_sequenceFrame);
	frame_number->blockSignals(true);
	frame_number->setValue(m_sequenceFrame);
	frame_number->blockSignals(false);
	capture_button->setEnabled(true);
    }
}

void StopmotionWidget::slotCaptureFrame()
{
    if (m_sequenceName != sequence_name->text()) {
	m_sequenceName = sequence_name->text();
	m_sequenceFrame = 0;
    }
    
    m_bmCapture->captureFrame(getPathForFrame(m_sequenceFrame));
    KNotification::event("FrameCaptured");
    frameoverlay_button->setEnabled(true);
    m_sequenceFrame++;
    frame_number->setRange(0, m_sequenceFrame);
    frame_number->blockSignals(true);
    frame_number->setValue(m_sequenceFrame);
    frame_number->blockSignals(false);
    button_addsequence->setEnabled(true);
    if (frameoverlay_button->isChecked()) QTimer::singleShot(1000, this, SLOT(slotUpdateOverlay()));
}

QString StopmotionWidget::getPathForFrame(int ix, QString seqName)
{
    if (seqName.isEmpty()) seqName = m_sequenceName;
    return m_projectFolder.path(KUrl::AddTrailingSlash) + seqName + "_" + QString::number(ix).rightJustified(4, '0', false) + ".png";
}

void StopmotionWidget::slotShowFrame(int ix)
{
    if (m_sequenceFrame == 0) {
	//there are no images in sequence
	return;
    }
    frameoverlay_button->blockSignals(true);
    frameoverlay_button->setChecked(false);
    frameoverlay_button->blockSignals(false);
    if (ix < m_sequenceFrame) {
	// Show previous frame
	QImage img(getPathForFrame(ix));
	capture_button->setEnabled(false);
	if (!img.isNull()) m_bmCapture->showOverlay(img, false);
    }
    else {
	m_bmCapture->hideOverlay();
	capture_button->setEnabled(true);
    }
    
}

void StopmotionWidget::slotAddSequence()
{
    emit addOrUpdateSequence(getPathForFrame(0));
}
