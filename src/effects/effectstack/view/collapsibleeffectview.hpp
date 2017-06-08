/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef COLLAPSIBLEEFFECTVIEW_H
#define COLLAPSIBLEEFFECTVIEW_H

#include "effectstack/abstractcollapsiblewidget.h"
#include "mltcontroller/effectscontroller.h"
#include "effectstack/parametercontainer.h"
#include "timecode.h"

#include <QDomElement>
#include <memory>

class QLabel;
class KDualAction;
class EffectItemModel;
class AssetParameterView;

/**)
 * @class CollapsibleEffectView
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class CollapsibleEffectView : public AbstractCollapsibleWidget
{
    Q_OBJECT

public:
    explicit CollapsibleEffectView(std::shared_ptr<EffectItemModel> effectModel,  QImage icon, QWidget *parent = nullptr);
    ~CollapsibleEffectView();
    QLabel *title;

    void setupWidget(const ItemInfo &info, EffectMetaInfo *metaInfo);
    void updateTimecodeFormat();
    void setActive(bool activate) override;
    /** @brief Install event filter so that scrolling with mouse wheel does not change parameter value. */
    bool eventFilter(QObject *o, QEvent *e) override;
    /** @brief Update effect GUI to reflect parameted changes. */
    void updateWidget(const ItemInfo &info, const QDomElement &effect, EffectMetaInfo *metaInfo);
    /** @brief Returns effect xml. */
    QDomElement effect() const;
    /** @brief Returns effect xml with keyframe offset for saving. */
    QDomElement effectForSave() const;
    int groupIndex() const;
    bool isGroup() const override;
    int effectIndex() const;
    void setGroupIndex(int ix);
    void setGroupName(const QString &groupName);
    /** @brief Remove this effect from its group. */
    void removeFromGroup();
    QString infoString() const;
    bool isActive() const;
    bool isEnabled() const;
    /** @brief Should the wheel event be sent to parent widget for scrolling. */
    bool filterWheelEvent;
    /** @brief Parent group was collapsed, update. */
    void groupStateChanged(bool collapsed);
    /** @brief Show / hide up / down buttons. */
    void adjustButtons(int ix, int max);
    /** @brief Returns this effect's monitor scene type if any is needed. */
    MonitorSceneType needsMonitorEffectScene() const;
    /** @brief Set clip in / out points. */
    void setRange(int inPoint, int outPoint);
    /** @brief Import keyframes from a clip's data. */
    void setKeyframes(const QString &tag, const QString &keyframes);
    /** @brief Pass frame size info (dar, etc). */
    void updateFrameInfo();
    /** @brief Select active keyframe. */
    void setActiveKeyframe(int kf);
    /** @brief Returns true if effect can be moved (false for speed effect). */
    bool isMovable() const;

public slots:
    void slotSyncEffectsPos(int pos);
    void slotDisable(bool disable, bool emitInfo = true);
    void slotResetEffect();
    void importKeyframes(const QString &keyframes);

private slots:
    void setWidgetHeight(qreal value);
    void animationFinished();

private slots:
    void slotSwitch(bool expand);
    void slotShow(bool show);
    void slotDeleteEffect();
    void slotEffectUp();
    void slotEffectDown();
    void slotSaveEffect();
    void slotCreateGroup();
    void slotCreateRegion();
    void slotUnGroup();
    /** @brief A sub effect parameter was changed */
    void slotUpdateRegionEffectParams(const QDomElement & /*old*/, const QDomElement & /*e*/, int /*ix*/);
    /** @brief Dis/enable effect before processing an operation (color picker) */
    void slotDisableEffect(bool disable);
    void prepareImportClipKeyframes();

private:
    ParameterContainer *m_paramWidget;
    AssetParameterView *m_view;
    std::shared_ptr<EffectItemModel> m_model;
    KDualAction *m_collapse;
    QList<CollapsibleEffectView *> m_subParamWidgets;
    QDomElement m_effect;
    ItemInfo m_itemInfo;
    QDomElement m_original_effect;
    QList<QDomElement> m_subEffects;
    QMenu *m_menu;
    QPoint m_clickPoint;
    EffectInfo m_info;
    bool m_isMovable;
    /** @brief True if this is a region effect, which behaves in a special way, like a group. */
    bool m_regionEffect;
    /** @brief The add group action. */
    QAction *m_groupAction;
    KDualAction *m_enabledButton;
    QLabel *m_colorIcon;
    QPixmap m_iconPix;
    /** @brief Check if collapsed state changed and inform MLT. */
    void updateCollapsedState();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

signals:
    void parameterChanged(const QDomElement &, const QDomElement &, int);
    void syncEffectsPos(int);
    void effectStateChanged(bool, int ix, MonitorSceneType effectNeedsMonitorScene);
    void deleteEffect(const QDomElement &);
    void activateEffect(int);
    void checkMonitorPosition(int);
    void seekTimeline(int);
    /** @brief Start an MLT filter job on this clip. */
    void startFilterJob(QMap<QString, QString> &, QMap<QString, QString> &, QMap<QString, QString> &);
    /** @brief An effect was reset, trigger param reload. */
    void resetEffect(int ix);
    /** @brief Ask for creation of a group. */
    void createGroup(int ix);
    void unGroup(CollapsibleEffectView *);
    void createRegion(int, const QUrl &);
    void deleteGroup(const QDomDocument &);
    void importClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, const QMap<QString, QString> &keyframes = QMap<QString, QString>());
};

#endif
