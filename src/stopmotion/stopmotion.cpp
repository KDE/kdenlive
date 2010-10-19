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
#include "../v4l/v4lcapture.h"
#include "../slideshowclip.h"
#include "kdenlivesettings.h"


#include <KDebug>
#include <KGlobalSettings>
#include <KFileDialog>
#include <KStandardDirs>
#include <KMessageBox>
#include <kdeversion.h>
#include <KNotification>

#include <QtConcurrentRun>
#include <QInputDialog>
#include <QComboBox>
#include <QVBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QAction>
#include <QWheelEvent>

MyLabel::MyLabel(QWidget *parent) :
    QLabel(parent)
{
}

void MyLabel::setImage(QImage img)
{
    m_img = img;
}

//virtual
void MyLabel::wheelEvent(QWheelEvent * event)
{
    if (event->delta() > 0) emit seek(true);
    else emit seek(false);
}

//virtual
void MyLabel::paintEvent(QPaintEvent * event)
{
    Q_UNUSED(event);

    QRect r(0, 0, width(), height());
    QPainter p(this);
    p.fillRect(r, QColor(KdenliveSettings::window_background()));
    double aspect_ratio = (double) m_img.width() / m_img.height();
    int pictureHeight = height();
    int pictureWidth = width();
    int calculatedWidth = aspect_ratio * height();
    if (calculatedWidth > width()) pictureHeight = width() / aspect_ratio;
    else {
        int calculatedHeight = width() / aspect_ratio;
        if (calculatedHeight > height()) pictureWidth = height() * aspect_ratio;
    }
    p.drawImage(QRect((width() - pictureWidth) / 2, (height() - pictureHeight) / 2, pictureWidth, pictureHeight), m_img, QRect(0, 0, m_img.width(), m_img.height()));
    p.end();
}


StopmotionWidget::StopmotionWidget(KUrl projectFolder, QWidget *parent) :
    QDialog(parent)
    , Ui::Stopmotion_UI()
    , m_projectFolder(projectFolder)
    , m_bmCapture(NULL)
    , m_sequenceFrame(0)
    , m_animatedIndex(-1)
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
    frameoverlay_button->setEnabled(false);
    removelast_button->setEnabled(false);
    capture_button->setEnabled(false);

    connect(sequence_name, SIGNAL(textChanged(const QString &)), this, SLOT(sequenceNameChanged(const QString &)));
    m_layout = new QVBoxLayout;
    if (BMInterface::getBlackMagicDeviceList(capture_device, NULL)) {
        // Found a BlackMagic device
        m_bmCapture = new BmdCaptureHandler(m_layout);
        connect(m_bmCapture, SIGNAL(gotMessage(const QString &)), this, SLOT(slotGotHDMIMessage(const QString &)));
    }
    if (QFile::exists(KdenliveSettings::video4vdevice())) {
        if (m_bmCapture == NULL) m_bmCapture = new V4lCaptureHandler(m_layout);
        capture_device->addItem(KdenliveSettings::video4vdevice(), "v4l");
    }
    connect(capture_device, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateHandler()));
    m_frame_preview = new MyLabel(this);
    connect(m_frame_preview, SIGNAL(seek(bool)), this, SLOT(slotSeekFrame(bool)));
    m_layout->addWidget(m_frame_preview);
    m_frame_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    video_preview->setLayout(m_layout);
    live_button->setChecked(false);
    frameoverlay_button->setChecked(false);
    button_addsequence->setEnabled(false);
    connect(live_button, SIGNAL(clicked(bool)), this, SLOT(slotLive(bool)));
    connect(frameoverlay_button, SIGNAL(clicked(bool)), this, SLOT(slotShowOverlay(bool)));
    connect(frame_number, SIGNAL(valueChanged(int)), this, SLOT(slotShowFrame(int)));
    connect(button_addsequence, SIGNAL(clicked(bool)), this, SLOT(slotAddSequence()));
    connect(preview_button, SIGNAL(clicked(bool)), this, SLOT(slotPlayPreview()));
    connect(frame_list, SIGNAL(currentRowChanged(int)), this, SLOT(slotShowSelectedFrame()));
    connect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));

    parseExistingSequences();
}

StopmotionWidget::~StopmotionWidget()
{
    m_bmCapture->stopPreview();
}

void StopmotionWidget::slotUpdateHandler()
{
    QString data = capture_device->itemData(capture_device->currentIndex()).toString();
    m_bmCapture->stopPreview();
    delete m_bmCapture;
    m_layout->removeWidget(m_frame_preview);
    if (data == "v4l") {
        m_bmCapture = new V4lCaptureHandler(m_layout);
    } else {
        m_bmCapture = new BmdCaptureHandler(m_layout);
        connect(m_bmCapture, SIGNAL(gotMessage(const QString &)), this, SLOT(slotGotHDMIMessage(const QString &)));
    }
    m_layout->addWidget(m_frame_preview);
}

void StopmotionWidget::slotGotHDMIMessage(const QString &message)
{
    log_box->insertItem(0, message);
}

void StopmotionWidget::parseExistingSequences()
{
    sequence_name->clear();
    sequence_name->addItem(QString());
    QDir dir(m_projectFolder.path());
    QStringList filters;
    filters << "*_0000.png";
    //dir.setNameFilters(filters);
    QStringList sequences = dir.entryList(filters, QDir::Files, QDir::Name);
    //kDebug()<<"PF: "<<<<", sm: "<<sequences;
    foreach(QString sequencename, sequences) {
        sequence_name->addItem(sequencename.section("_", 0, -2));
    }
}

void StopmotionWidget::slotLive(bool isOn)
{
    if (isOn) {
        m_frame_preview->setImage(QImage());
        m_frame_preview->setHidden(true);
        m_bmCapture->startPreview(KdenliveSettings::hdmi_capturedevice(), KdenliveSettings::hdmi_capturemode());
        capture_button->setEnabled(true);
    } else {
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
    } else {
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
    // Get rid of frames from previous sequence
    disconnect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));
    m_filesList.clear();
    m_future.waitForFinished();
    frame_list->clear();
    if (name.isEmpty()) {
        button_addsequence->setEnabled(false);
        frame_number->blockSignals(true);
        frame_number->setValue(m_sequenceFrame);
        frame_number->blockSignals(false);
        frameoverlay_button->setEnabled(false);
        removelast_button->setEnabled(false);
    } else {
        // Check if we are editing an existing sequence
        QString pattern = SlideshowClip::selectedPath(getPathForFrame(0, sequence_name->currentText()), false, QString(), &m_filesList);
        m_sequenceFrame = m_filesList.size();
        frame_number->blockSignals(true);
        frame_number->setValue(0);
        frame_number->blockSignals(false);
        m_currentIndex = 0;
        if (!m_filesList.isEmpty()) {
            m_sequenceName = sequence_name->currentText();
            connect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));
            m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
            button_addsequence->setEnabled(true);
            frameoverlay_button->setEnabled(true);
        } else {
            button_addsequence->setEnabled(false);
            frameoverlay_button->setEnabled(false);
        }
        frame_number->setRange(0, m_sequenceFrame);
        capture_button->setEnabled(true);
    }
}

void StopmotionWidget::slotCaptureFrame()
{
    if (sequence_name->currentText().isEmpty()) {
        QString seqName = QInputDialog::getText(this, i18n("Create New Sequence"), i18n("Enter sequence name"));
        if (seqName.isEmpty()) return;
        sequence_name->blockSignals(true);
        sequence_name->setItemText(sequence_name->currentIndex(), seqName);
        sequence_name->blockSignals(false);
    }
    if (m_sequenceName != sequence_name->currentText()) {
        m_sequenceName = sequence_name->currentText();
        m_sequenceFrame = 0;
    }
    capture_button->setEnabled(false);
    m_bmCapture->captureFrame(getPathForFrame(m_sequenceFrame));
    KNotification::event("FrameCaptured");
    frameoverlay_button->setEnabled(true);
    m_sequenceFrame++;
    frame_number->setRange(0, m_sequenceFrame);
    frame_number->blockSignals(true);
    frame_number->setValue(m_sequenceFrame);
    frame_number->blockSignals(false);
    button_addsequence->setEnabled(true);
    //if (frameoverlay_button->isChecked()) QTimer::singleShot(1000, this, SLOT(slotUpdateOverlay()));
    QTimer::singleShot(1000, this, SLOT(slotUpdateFrameList()));
}

void StopmotionWidget::slotPrepareThumbs()
{
    if (m_filesList.isEmpty()) return;
    QString path = m_filesList.takeFirst();
    emit doCreateThumbs(QImage(path), SlideshowClip::getFrameNumberFromPath(path));

}

void StopmotionWidget::slotCreateThumbs(QImage img, int ix)
{
    if (img.isNull()) {
        m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
        return;
    }
    int height = 90;
    int width = height * img.width() / img.height();
    frame_list->setIconSize(QSize(width, height));
    QPixmap pix = QPixmap::fromImage(img).scaled(width, height);
    QString nb = QString::number(ix);
    QPainter p(&pix);
    QFontInfo finfo(font());
    p.fillRect(0, 0, finfo.pixelSize() * nb.count() + 6, finfo.pixelSize() + 6, QColor(0, 0, 0, 150));
    p.setPen(Qt::white);
    p.drawText(QPoint(3, finfo.pixelSize() + 3), nb);
    p.end();
    QIcon icon(pix);
    QListWidgetItem *item = new QListWidgetItem(icon, QString(), frame_list);
    item->setToolTip(getPathForFrame(ix, sequence_name->currentText()));
    item->setData(Qt::UserRole, ix);
    m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
}

void StopmotionWidget::slotUpdateFrameList(int ix)
{
    kDebug() << "// GET FRAME: " << ix;
    if (ix == -1) ix = m_sequenceFrame - 1;
    QString path = getPathForFrame(ix);
    if (!QFile::exists(path)) {
        capture_button->setEnabled(true);
        return;
    }
    QImage img(path);
    if (img.isNull()) {
        if (ix == m_sequenceFrame - 1) QTimer::singleShot(1000, this, SLOT(slotUpdateFrameList()));
        return;
    }
    int height = 90;
    int width = height * img.width() / img.height();
    frame_list->setIconSize(QSize(width, height));
    QPixmap pix = QPixmap::fromImage(img).scaled(width, height);
    QString nb = QString::number(ix);
    QPainter p(&pix);
    QFontInfo finfo(font());
    p.fillRect(0, 0, finfo.pixelSize() * nb.count() + 6, finfo.pixelSize() + 6, QColor(0, 0, 0, 150));
    p.setPen(Qt::white);
    p.drawText(QPoint(3, finfo.pixelSize() + 3), nb);
    p.end();
    QIcon icon(pix);
    QListWidgetItem *item = new QListWidgetItem(icon, QString(), frame_list);
    item->setData(Qt::UserRole, ix);
    frame_list->blockSignals(true);
    frame_list->setCurrentItem(item);
    frame_list->blockSignals(true);
    capture_button->setEnabled(true);
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
        slotLive(false);
        live_button->setChecked(false);
        QImage img(getPathForFrame(ix));
        capture_button->setEnabled(false);
        if (!img.isNull()) {
            //m_bmCapture->showOverlay(img, false);
            m_bmCapture->hidePreview(true);
            m_frame_preview->setImage(img);
            m_frame_preview->setHidden(false);
            m_frame_preview->update();
            selectFrame(ix);
        }
    }
    /*else {
    slotLive(true);
    m_frame_preview->setImage(QImage());
    m_frame_preview->setHidden(true);
    m_bmCapture->hideOverlay();
    m_bmCapture->hidePreview(false);
    capture_button->setEnabled(true);
    }*/
}

void StopmotionWidget::slotShowSelectedFrame()
{
    QListWidgetItem *item = frame_list->currentItem();
    if (item) {
        int ix = item->data(Qt::UserRole).toInt();
        //frame_number->blockSignals(true);
        frame_number->setValue(ix);
        //frame_number->blockSignals(false);
        //slotShowFrame(ix);
    }
}

void StopmotionWidget::slotAddSequence()
{
    emit addOrUpdateSequence(getPathForFrame(0));
}

void StopmotionWidget::slotPlayPreview()
{
    if (m_animatedIndex != -1) {
        // stop animation
        m_animatedIndex = -1;
        return;
    }
    QListWidgetItem *item = frame_list->currentItem();
    if (item) {
        m_animatedIndex = item->data(Qt::UserRole).toInt();
    }
    QTimer::singleShot(200, this, SLOT(slotAnimate()));
}

void StopmotionWidget::slotAnimate()
{
    slotShowFrame(m_animatedIndex);
    m_animatedIndex++;
    if (m_animatedIndex < m_sequenceFrame) QTimer::singleShot(200, this, SLOT(slotAnimate()));
    else m_animatedIndex = -1;
}

QListWidgetItem *StopmotionWidget::getFrameFromIndex(int ix)
{
    QListWidgetItem *item = NULL;
    int pos = ix;
    if (ix >= frame_list->count()) {
        pos = frame_list->count() - 1;
    }
    if (ix < 0) pos = 0;
    item = frame_list->item(pos);

    int value = item->data(Qt::UserRole).toInt();
    if (value == ix) return item;
    else if (value < ix) {
        pos++;
        while (pos < frame_list->count()) {
            item = frame_list->item(pos);
            value = item->data(Qt::UserRole).toInt();
            if (value == ix) return item;
            pos++;
        }
    } else {
        pos --;
        while (pos >= 0) {
            item = frame_list->item(pos);
            value = item->data(Qt::UserRole).toInt();
            if (value == ix) return item;
            pos --;
        }
    }
    return NULL;
}


void StopmotionWidget::selectFrame(int ix)
{
    frame_list->blockSignals(true);
    QListWidgetItem *item = getFrameFromIndex(ix);
    if (!item) return;
    frame_list->setCurrentItem(item);
    frame_list->blockSignals(false);
}

void StopmotionWidget::slotSeekFrame(bool forward)
{
    int ix = frame_list->currentRow();
    if (forward) {
        if (ix < frame_list->count() - 1) frame_list->setCurrentRow(ix + 1);
    } else if (ix > 0) frame_list->setCurrentRow(ix - 1);
}


