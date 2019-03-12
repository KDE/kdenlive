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

#include <QDomElement>
#include <QList>
#include <QWidget>

#include "assets/view/widgets/abstractparamwidget.hpp"
#include "mlt++/MltAnimation.h"
#include "mlt++/MltProperties.h"
#include "timecodedisplay.h"

class AnimKeyframeRuler;
class Monitor;
class KDualAction;
struct EffectMetaInfo;
class DoubleWidget;
class KSelectAction;
class QComboBox;
class DragValue;

class AnimationWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    AnimationWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QPair<int, int> range, QWidget *parent);

    // explicit AnimationWidget(EffectMetaInfo *info, int clipPos, int min, int max, int effectIn, const QString &effectId, const QDomElement &xml,QWidget
    // *parent = nullptr);
    virtual ~AnimationWidget();
    void updateTimecodeFormat();
    void addParameter(QModelIndex ix);
    const QMap<QString, QString> getAnimation();
    static QString getDefaultKeyframes(int start, const QString &defaultValue, bool linearOnly = false);
    void setActiveKeyframe(int frame);
    void finishSetup();
    /** @brief Returns true if currently active param is name */
    bool isActive(const QString &name) const;
    /** @brief Effect was selected / deselected, update monitor connections */
    void connectMonitor(bool activate);
    /** @brief Adds an offset to all keyframes (translate by x frames) */
    void offsetAnimation(int offset);
    /** @brief Effect param @tag was changed, reload keyframes */
    void reload(const QString &tag, const QString &data);

private:
    AnimKeyframeRuler *m_ruler;
    Monitor *m_monitor;
    QSize m_frameSize;
    QSize m_monitorSize;
    TimecodeDisplay *m_timePos;
    KDualAction *m_addKeyframe;
    QComboBox *m_presetCombo;
    QAction *m_previous;
    QAction *m_next;
    /** @brief True if effect is currently selected in stack */
    bool m_active;
    int m_clipPos;
    int m_inPoint;
    int m_outPoint;
    /** @brief the keyframe position currently edited in slider */
    int m_editedKeyframe;
    /** @brief name of currently active animated parameter */
    QString m_inTimeline;
    /** @brief name of geometry animated parameter */
    QString m_rectParameter;
    /** @brief the keyframe position which should be attached to end (negative frame) */
    int m_attachedToEnd;
    QDomElement m_xml;
    QList<QPair<QModelIndex, QString>> m_parameters;
    QString m_effectId;
    Mlt::Animation m_animController;
    Mlt::Properties m_animProperties;
    KSelectAction *m_selectType;
    QAction *m_endAttach;
    QList<QDomElement> m_params;
    QMap<QString, DoubleWidget *> m_doubleWidgets;
    DragValue *m_spinX;
    DragValue *m_spinY;
    DragValue *m_spinWidth;
    DragValue *m_spinHeight;
    DragValue *m_spinSize;
    DragValue *m_spinOpacity;
    QAction *m_originalSize;
    QAction *m_lockRatio;
    int m_offset;
    void parseKeyframes();
    void rebuildKeyframes();
    void updateToolbar();
    void loadPresets(QString currentText = QString());
    void loadPreset(const QString &path);
    /** @brief update the parameter slider to reflect value of current position / selected keyframe */
    void updateSlider(int pos);
    void updateRect(int pos);
    /** @brief Create widget to adjust param value */
    void buildSliderWidget(const QString &paramTag, QModelIndex ix);
    void buildRectWidget(const QString &paramTag, QModelIndex ix);
    /** @brief Calculate path for keyframes centers and send to monitor */
    void setupMonitor(QRect r = QRect());
    /** @brief Returns the default value for a parameter from its name */
    QString defaultValue(const QString &paramName);
    /** @brief Add a keyframe in all geometries */
    void doAddKeyframe(int pos, QString paramName, bool directUpdate);

public slots:
    void slotSyncPosition(int relTimelinePos);
    void slotPositionChanged(int pos = -1, bool seek = true);
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

    /** @brief update the clip's in/out point
     */
    void slotSetRange(QPair<int, int>) override;

private slots:
    void slotPrevious();
    void slotNext();
    void slotAddDeleteKeyframe(bool add, int pos = -1);
    void moveKeyframe(int oldPos, int newPos);
    void slotEditKeyframeType(QAction *action);
    void slotAdjustKeyframeValue(double value);
    void slotAdjustRectKeyframeValue();
    void slotAddKeyframe(int pos = -1);
    void slotDeleteKeyframe(int pos = -1);
    void slotReverseKeyframeType(bool reverse);
    void applyPreset(int ix);
    void savePreset();
    void deletePreset();
    void slotCopyKeyframes();
    void slotImportKeyframes();
    void slotSetDefaultInterp(QAction *action);
    void slotUpdateVisibleParameter(bool display);
    void slotUpdateGeometryRect(const QRect r);
    void slotUpdateCenters(const QVariantList &centers);
    void slotSeekToKeyframe(int ix);
    void slotAdjustToSource();
    void slotAdjustToFrameSize();
    void slotFitToWidth();
    void slotFitToHeight();
    void slotResize(double value);
    /** @brief Delete all keyframes after current cursor pos. */
    void slotRemoveNext();

    /** @brief Moves the rect to the left frame border (x position = 0). */
    void slotMoveLeft();
    /** @brief Centers the rect horizontally. */
    void slotCenterH();
    /** @brief Moves the rect to the right frame border (x position = frame width - rect width). */
    void slotMoveRight();
    /** @brief Moves the rect to the top frame border (y position = 0). */
    void slotMoveTop();
    /** @brief Centers the rect vertically. */
    void slotCenterV();
    /** @brief Moves the rect to the bottom frame border (y position = frame height - rect height). */
    void slotMoveBottom();
    /** @brief Un/Lock aspect ratio for size in effect parameter. */
    void slotLockRatio();
    void slotAdjustRectHeight();
    void slotAdjustRectWidth();
    /** @brief Seek monitor. */
    void requestSeek(int pos);
    /** @brief monitor seek pos changed. */
    void monitorSeek(int pos);

signals:
    /** @brief keyframes dropped / pasted on widget, import them. */
    void setKeyframes(const QString &);
};

#endif
