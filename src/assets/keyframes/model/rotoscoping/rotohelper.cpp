/*
SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "rotohelper.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "monitor/monitor.h"
#include "utils/gentime.h"

#include <QSize>
#include <utility>
RotoHelper::RotoHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, QPersistentModelIndex index, QObject *parent)
    : KeyframeMonitorHelper(monitor, std::move(model), index, parent)
{
}

void RotoHelper::slotUpdateFromMonitorData(const QVariantList &v)
{
    const QVariant res = RotoHelper::getSpline(QVariant(v), pCore->getCurrentFrameSize());
    Q_EMIT updateKeyframeData(m_indexes.first(), res);
}

QVariant RotoHelper::getSpline(const QVariant &value, const QSize frame)
{
    QList<BPoint> bPoints;
    const QVariantList points = value.toList();
    for (int i = 0; i < points.size() / 3; i++) {
        BPoint b(points.at(3 * i).toPointF(), points.at(3 * i + 1).toPointF(), points.at(3 * i + 2).toPointF());
        bPoints << b;
    }
    QList<QVariant> vlist;
    for (const BPoint &point : bPoints) {
        QList<QVariant> pl;
        for (int i = 0; i < 3; ++i) {
            pl << QVariant(QList<QVariant>() << QVariant(point[i].x() / frame.width()) << QVariant(point[i].y() / frame.height()));
        }
        vlist << QVariant(pl);
    }
    return vlist;
}

void RotoHelper::refreshParams(int pos)
{
    QVariantList centerPoints;
    QVariantList controlPoints;
    std::shared_ptr<KeyframeModelList> keyframes = m_model->getKeyframeModel();
    if (!keyframes->isEmpty()) {
        QVariant splineData = keyframes->getInterpolatedValue(pos, m_indexes.first());
        QList<BPoint> p = getPoints(splineData, pCore->getCurrentFrameSize());
        for (const auto &i : qAsConst(p)) {
            centerPoints << QVariant(i.p);
            controlPoints << QVariant(i.h1);
            controlPoints << QVariant(i.h2);
        }
    }
    if (m_monitor && keyframes->count() > 0) {
        m_monitor->setUpEffectGeometry(QRect(), centerPoints, controlPoints);
    }
}

QList<BPoint> RotoHelper::getPoints(const QVariant &value, const QSize frame)
{
    QList<BPoint> points;
    QList<QVariant> data = value.toList();

    // skip tracking flag
    if (data.count() && data.at(0).canConvert<QString>()) {
        data.removeFirst();
    }

    for (const QVariant &bpoint : data) {
        QList<QVariant> l = bpoint.toList();
        BPoint p;
        for (int i = 0; i < 3; ++i) {
            p[i] = QPointF(l.at(i).toList().at(0).toDouble() * frame.width(), l.at(i).toList().at(1).toDouble() * frame.height());
        }
        points << p;
    }
    return points;
}
