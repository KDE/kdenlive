/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef ANIMATIONWIDGET_H
#define ANIMATIONWIDGET_H

#include <QWidget>
#include <QList>

#include "timecodedisplay.h"
#include "mlt++/MltProperties.h"

class AnimKeyframeRuler;
class Monitor;
class KDualAction;
class EffectMetaInfo;
class DoubleParameterWidget;
class KSelectAction;
class QDialog;
class QComboBox;

namespace Mlt {
    class Animation;
}

class AnimationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AnimationWidget(EffectMetaInfo *info, int clipPos, int min, int max, const QString &effectId, QDomElement xml, int activeKeyframe, QWidget *parent = 0);
    virtual ~AnimationWidget();
    void updateTimecodeFormat();
    void addParameter(const QDomElement &e, int activeKeyframe);
    const QString getAnimation();
    static QString getDefaultKeyframes(const QString &defaultValue);
    void setActiveKeyframe(int frame);

private:
    AnimKeyframeRuler *m_ruler;
    Monitor *m_monitor;
    TimecodeDisplay *m_timePos;
    KDualAction* m_addKeyframe;
    QComboBox *m_presetCombo;
    int m_clipPos;
    int m_inPoint;
    int m_outPoint;
    double m_factor;
    /** @brief the keyframe position currently edited in slider */
    int m_editedKeyframe;
    /** @brief the keyframe position which should be attached to end (negative frame) */
    int m_attachedToEnd;
    QDomElement m_xml;
    QString m_effectId;
    Mlt::Animation m_animController;
    Mlt::Properties m_animProperties;
    KSelectAction *m_selectType;
    QAction *m_endAttach;
    QList <QDomElement> m_params;
    QList <DoubleParameterWidget *> m_doubleWidgets;
    void parseKeyframes();
    void rebuildKeyframes();
    void updateToolbar();
    void loadPresets(QString currentText = QString());
    void loadPreset(const QString &path);
    /** @brief update the parameter slider to reflect value of current position / selected keyframe */
    void updateSlider(int pos);

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent * event);

public slots:
    void slotSyncPosition(int relTimelinePos);

private slots:
    void slotPrevious();
    void slotNext();
    void slotAddDeleteKeyframe(bool add);
    void moveKeyframe(int oldPos, int newPos);
    void slotEditKeyframeType(QAction *action);
    void slotAdjustKeyframeValue(double value);
    void slotPositionChanged(int pos = -1, bool seek = true);
    void slotAddKeyframe(int);
    void slotDeleteKeyframe(int);
    void slotReverseKeyframeType(bool reverse);
    void applyPreset(int ix);
    void savePreset();
    void deletePreset();
    void slotSetDefaultInterp(QAction *action);

signals:
    void seekToPos(int);
    void parameterChanged();
};

#endif
