/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "rotowidget.h"
#include "monitor.h"
#include "renderer.h"
#include "monitorscene.h"
#include "monitoreditwidget.h"
#include "onmonitoritems/rotoscoping/bpointitem.h"
#include "onmonitoritems/rotoscoping/splineitem.h"
#include "simplekeyframes/simplekeyframewidget.h"
#include "kdenlivesettings.h"

#include <math.h>

#include <qjson/parser.h>
#include <qjson/serializer.h>

#include <QVBoxLayout>


RotoWidget::RotoWidget(QString data, Monitor *monitor, int in, int out, Timecode t, QWidget* parent) :
        QWidget(parent),
        m_monitor(monitor),
        m_showScene(true),
        m_in(in),
        m_out(out),
        m_pos(0)
{
    QVBoxLayout *l = new QVBoxLayout(this);
    m_keyframeWidget = new SimpleKeyframeWidget(t, m_out - m_in - 1, this);
    l->addWidget(m_keyframeWidget);

    MonitorEditWidget *edit = monitor->getEffectEdit();
    edit->showVisibilityButton(true);
    m_scene = edit->getScene();

    QJson::Parser parser;
    bool ok;
    m_data = parser.parse(data.toUtf8(), &ok);
    if (!ok) {
        // :(
    }

    
    if (m_data.canConvert(QVariant::Map)) {
        QList <int> keyframes;
        QMap <QString, QVariant> map = m_data.toMap();
        QMap <QString, QVariant>::const_iterator i = map.constBegin();
        while (i != map.constEnd()) {
            keyframes.append(i.key().toInt() - m_in);
            ++i;
        }
        m_keyframeWidget->setKeyframes(keyframes);

        for (int j = 0; j < keyframes.count(); ++j) {
            // key might already be justified
            if (map.contains(QString::number(keyframes.at(j) + m_in))) {
                QVariant value = map.take(QString::number(keyframes.at(j) + m_in));
                map[QString::number(keyframes.at(j) + m_in).rightJustified(qRound(log10((double)m_out)), '0')] = value;
            }
        }
        m_data = QVariant(map);
    } else {
        m_keyframeWidget->setKeyframes(QList <int>() << 0);
    }

    m_item = new SplineItem(QList <BPoint>(), NULL, m_scene);

    connect(m_item, SIGNAL(changed(bool)), this, SLOT(slotUpdateData(bool)));
    connect(edit, SIGNAL(showEdit(bool)), this, SLOT(slotShowScene(bool)));
    connect(m_monitor, SIGNAL(renderPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
    connect(m_keyframeWidget, SIGNAL(positionChanged(int)), this, SLOT(slotPositionChanged(int)));
    connect(m_keyframeWidget, SIGNAL(keyframeAdded(int)), this, SLOT(slotAddKeyframe(int)));
    connect(m_keyframeWidget, SIGNAL(keyframeRemoved(int)), this, SLOT(slotRemoveKeyframe(int)));
    connect(m_keyframeWidget, SIGNAL(keyframeMoved(int,int)), this, SLOT(slotMoveKeyframe(int,int)));
    connect(m_scene, SIGNAL(addKeyframe()), this, SLOT(slotAddKeyframe()));

    slotPositionChanged(0, false);
}

RotoWidget::~RotoWidget()
{
    delete m_keyframeWidget;

    m_scene->removeItem(m_item);
    delete m_item;

    if (m_monitor) {
        MonitorEditWidget *edit = m_monitor->getEffectEdit();
        edit->showVisibilityButton(false);
        edit->removeCustomControls();
        m_monitor->slotEffectScene(false);
    }
}

void RotoWidget::slotCheckMonitorPosition(int renderPos)
{
    if (m_showScene)
        emit checkMonitorPosition(renderPos);
}

void RotoWidget::slotSyncPosition(int relTimelinePos)
{
    relTimelinePos = qBound(0, relTimelinePos, m_out);
    m_keyframeWidget->slotSetPosition(relTimelinePos, false);
    slotPositionChanged(relTimelinePos, false);
}

void RotoWidget::slotShowScene(bool show)
{
    m_showScene = show;
    if (!m_showScene)
        m_monitor->slotEffectScene(false);
    else
        slotCheckMonitorPosition(m_monitor->render->seekFramePosition());
}

void RotoWidget::slotUpdateData(int pos, bool editing)
{
    Q_UNUSED(editing)

    int width = m_monitor->render->frameRenderWidth();
    int height = m_monitor->render->renderHeight();

    QList <BPoint> spline = m_item->getPoints();
    QList <QVariant> vlist;
    foreach (const BPoint &point, spline) {
        QList <QVariant> pl;
        for (int i = 0; i < 3; ++i)
            pl << QVariant(QList <QVariant>() << QVariant(point[i].x() / width) << QVariant(point[i].y() / height));
        vlist << QVariant(pl);
    }

    if (m_data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> map = m_data.toMap();
        map[QString::number((pos < 0 ? m_keyframeWidget->getPosition() : pos) + m_in).rightJustified(qRound(log10((double)m_out)), '0')] = QVariant(vlist);
        m_data = QVariant(map);
    } else {
        m_data = QVariant(vlist);
    }

    emit valueChanged();
}

void RotoWidget::slotUpdateData(bool editing)
{
    slotUpdateData(-1, editing);
}

QString RotoWidget::getSpline()
{
    QJson::Serializer serializer;
    return QString(serializer.serialize(m_data));
}

void RotoWidget::slotPositionChanged(int pos, bool seek)
{
    if (m_item->editing())
        return;

    m_keyframeWidget->slotSetPosition(pos, false);

    pos += m_in;

    if (m_data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> map = m_data.toMap();
        QMap <QString, QVariant>::const_iterator i = map.constBegin();
        int keyframe1, keyframe2;
        keyframe1 = keyframe2 = i.key().toInt();
        while (i.key().toInt() < pos && ++i != map.constEnd()) {
            keyframe1 = keyframe2;
            keyframe2 = i.key().toInt();
        }

        if (keyframe1 != keyframe2 && pos < keyframe2) {
            QList <BPoint> p1 = getPoints(keyframe1);
            QList <BPoint> p2 = getPoints(keyframe2);
            QList <BPoint> p;
            qreal relPos = (pos - keyframe1) / (qreal)(keyframe2 - keyframe1 + 1);

            int count = qMin(p1.count(), p2.count());
            for (int i = 0; i < count; ++i) {
                BPoint bp;
                for (int j = 0; j < 3; ++j) {
                    if (p1.at(i)[j] != p2.at(i)[j])
                        bp[j] = QLineF(p1.at(i)[j], p2.at(i)[j]).pointAt(relPos);
                    else
                        bp[j] = p1.at(i)[j];
                }
                p.append(bp);
            }

            m_item->setPoints(p);
            m_item->setEnabled(false);
            m_scene->setEnabled(false);
        } else {
            m_item->setPoints(getPoints(keyframe2));
            m_item->setEnabled(pos == keyframe2);
            m_scene->setEnabled(pos == keyframe2);
        }
    } else {
        m_item->setPoints(getPoints(-1));
        m_item->setEnabled(true);
        m_scene->setEnabled(true);
    }

    if (seek)
        emit seekToPos(pos - m_in);
}

QList <BPoint> RotoWidget::getPoints(int keyframe)
{
    int width = m_monitor->render->frameRenderWidth();
    int height = m_monitor->render->renderHeight();
    QList <BPoint> points;
    QList <QVariant> data;
    if (keyframe >= 0)
        data = m_data.toMap()[QString::number(keyframe).rightJustified(qRound(log10((double)m_out)), '0')].toList();
    else
        data = m_data.toList();
    foreach (const QVariant &bpoint, data) {
        QList <QVariant> l = bpoint.toList();
        BPoint p;
        p.h1 = QPointF(l.at(0).toList().at(0).toDouble() * width, l.at(0).toList().at(1).toDouble() * height);
        p.p = QPointF(l.at(1).toList().at(0).toDouble() * width, l.at(1).toList().at(1).toDouble() * height);
        p.h2 = QPointF(l.at(2).toList().at(0).toDouble() * width, l.at(2).toList().at(1).toDouble() * height);
        points << p;
    }
    return points;
}

void RotoWidget::slotAddKeyframe(int pos)
{
    if (!m_data.canConvert(QVariant::Map)) {
        QVariant data = m_data;
        QMap<QString, QVariant> map;
        map[QString::number(m_in).rightJustified(qRound(log10((double)m_out)), '0')] = data;
        m_data = QVariant(map);
    }

    if (pos < 0)
        m_keyframeWidget->addKeyframe();

    slotUpdateData(pos);
    m_item->setEnabled(true);
    m_scene->setEnabled(true);
}

void RotoWidget::slotRemoveKeyframe(int pos)
{
    if (pos < 0)
        pos = m_keyframeWidget->getPosition();

    if (!m_data.canConvert(QVariant::Map) || m_data.toMap().count() < 2)
        return;

    QMap<QString, QVariant> map = m_data.toMap();
    map.remove(QString::number(pos + m_in).rightJustified(qRound(log10((double)m_out)), '0'));
    m_data = QVariant(map);

    if (m_data.toMap().count() == 1)
        m_data = m_data.toMap().begin().value();

    slotPositionChanged(m_keyframeWidget->getPosition(), false);
    emit valueChanged();
}

void RotoWidget::slotMoveKeyframe(int oldPos, int newPos)
{
    if (m_data.canConvert(QVariant::Map)) {
        QMap<QString, QVariant> map = m_data.toMap();
        map[QString::number(newPos + m_in).rightJustified(qRound(log10((double)m_out)), '0')] = map.take(QString::number(oldPos + m_in).rightJustified(qRound(log10((double)m_out)), '0'));
        m_data = QVariant(map);
    }

    slotPositionChanged(m_keyframeWidget->getPosition(), false);
    emit valueChanged();
}

#include "rotowidget.moc"
