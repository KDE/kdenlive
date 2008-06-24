
#include <QMouseEvent>
#include <QStylePainter>
#include <QFrame>
#include <QWidget>
#include <QPainter>

#include <KLocale>
#include <KDebug>

#include "kdenlivesettings.h"
#include "headertrack.h"

HeaderTrack::HeaderTrack(int index, TrackInfo info, QWidget *parent)
        : QWidget(parent), m_index(index), m_type(info.type) {
    setFixedHeight(KdenliveSettings::trackheight());
    view.setupUi(this);
    view.track_number->setText(QString::number(m_index));
    if (m_type == VIDEOTRACK) {
        view.frame->setBackgroundRole(QPalette::AlternateBase);
        view.frame->setAutoFillBackground(true);
		view.buttonVideo->setIcon(KIcon("kdenlive-show-video"));
    } else {
        view.buttonVideo->setHidden(true);
    }
	view.buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    view.buttonVideo->setChecked(!info.isBlind);
    view.buttonAudio->setChecked(!info.isMute);
    connect(view.buttonVideo, SIGNAL(clicked()), this, SLOT(switchVideo()));
    connect(view.buttonAudio, SIGNAL(clicked()), this, SLOT(switchAudio()));
}

void HeaderTrack::switchVideo() {
    emit switchTrackVideo(m_index);
}

void HeaderTrack::switchAudio() {
    emit switchTrackAudio(m_index);
}

// virtual
/*void HeaderTrack::paintEvent(QPaintEvent *e) {
    QRect region = e->rect();
    region.setTopLeft(QPoint(region.left() + 1, region.top() + 1));
    region.setBottomRight(QPoint(region.right() - 1, region.bottom() - 1));
    QPainter painter(this);
    if (m_type == AUDIOTRACK) painter.fillRect(region, QBrush(QColor(240, 240, 255)));
    else painter.fillRect(region, QBrush(QColor(255, 255, 255)));
    painter.drawText(region, Qt::AlignCenter, m_label);
}*/


#include "headertrack.moc"
