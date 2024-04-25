/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractcollapsiblewidget.h"
#include "definitions.h"
#include "utils/timecode.h"

#include <QDomElement>
#include <memory>

class QLabel;
class KDualAction;
class KSqueezedTextLabel;
class EffectItemModel;
class AssetParameterView;
class TimecodeDisplay;

/**)
 * @class CollapsibleEffectView
 * @brief A container for the parameters of an effect
 * @author Jean-Baptiste Mardelle
 */
class CollapsibleEffectView : public AbstractCollapsibleWidget
{
    Q_OBJECT

public:
    explicit CollapsibleEffectView(const QString &effectName, const std::shared_ptr<EffectItemModel> &effectModel, QSize frameSize, QWidget *parent = nullptr);
    ~CollapsibleEffectView() override;
    KSqueezedTextLabel *title;

    void updateTimecodeFormat();
    /** @brief Install event filter so that scrolling with mouse wheel does not change parameter value. */
    bool eventFilter(QObject *o, QEvent *e) override;
    /** @brief Update effect GUI to reflect parameted changes. */
    void updateWidget(const ItemInfo &info, const QDomElement &effect);
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
    /** @brief Show / hide up / down buttons. */
    void adjustButtons(int ix, int max);
    /** @brief Returns this effect's monitor scene type if any is needed. */
    MonitorSceneType needsMonitorEffectScene() const;
    /** @brief Import keyframes from a clip's data. */
    void setKeyframes(const QString &tag, const QString &keyframes);
    /** @brief Pass frame size info (dar, etc). */
    void updateFrameInfo();
    /** @brief Select active keyframe. */
    void setActiveKeyframe(int kf);
    /** @brief Returns true if effect can be moved (false for speed effect). */
    bool isMovable() const;
    /** @brief Returns the effect in xml format for saving. */
    QDomDocument toXml() const;
    /** @brief Update monitor scene depending on effect enabled state. */
    void updateScene();
    void updateInOut(QPair<int, int> inOut, bool withUndo);
    void slotNextKeyframe();
    void slotPreviousKeyframe();
    void addRemoveKeyframe();
    /** @brief Used to pass a standard action like copy or paste to the effect stack widget */
    void sendStandardCommand(int command);

public Q_SLOTS:
    void slotSyncEffectsPos(int pos);
    /** @brief Enable / disable an effect. */
    void slotDisable(bool disable);
    /** @brief Restrict an effec to an in/out point region, of full length. */
    void switchInOut(bool checked);
    void slotResetEffect();
    void importKeyframes(const QString &keyframes);
    void slotActivateEffect(bool active);
    void updateHeight();
    /** @brief Should we block wheel event (if parent is a view with scrollbar) */
    void blockWheelEvent(bool block);
    /** @brief Switch between collapsed/expanded state */
    void switchCollapsed(int row);
    /** @brief Open a save effect dialog */
    void slotSaveEffect();
    /** @brief Show hide the count of grouped instances for this effect */
    void updateGroupedInstances();

private Q_SLOTS:
    void setWidgetHeight(qreal value);
    void enableView(bool enabled);
    void enableHideKeyframes(bool enabled);
    void slotSwitch(bool expand);
    void slotDeleteEffect();
    void slotEffectUp();
    void slotEffectDown();
    void slotCreateGroup();
    void slotCreateRegion();
    void slotUnGroup();
    /** @brief A sub effect parameter was changed */
    void slotUpdateRegionEffectParams(const QDomElement & /*old*/, const QDomElement & /*e*/, int /*ix*/);
    void prepareImportClipKeyframes();
    void updateEffectZone();
    void slotHideKeyframes(bool hide);

private:
    AssetParameterView *m_view;
    std::shared_ptr<EffectItemModel> m_model;
    KDualAction *m_collapse;
    QList<CollapsibleEffectView *> m_subParamWidgets;
    QDomElement m_effect;
    ItemInfo m_itemInfo;
    QDomElement m_original_effect;
    QList<QDomElement> m_subEffects;
    QLabel *m_effectInstances{nullptr};
    QMenu *m_menu;
    bool m_isMovable;
    bool m_blockWheel;
    /** @brief The add group action. */
    QAction *m_groupAction;
    KDualAction *m_enabledButton;
    KDualAction *m_keyframesButton;
    QAction *m_inOutButton;
    QLabel *m_colorIcon;
    QPixmap m_iconPix;
    QPoint m_dragStart;
    TimecodeDisplay *m_inPos;
    TimecodeDisplay *m_outPos;
    bool m_dragging;

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void leaveEvent(QEvent *event) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEvent *event) override;
#else
    void enterEvent(QEnterEvent *event) override;
#endif
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

Q_SIGNALS:
    void parameterChanged(const QDomElement &, const QDomElement &, int);
    void syncEffectsPos(int);
    void effectStateChanged(bool, int ix, MonitorSceneType effectNeedsMonitorScene);
    void deleteEffect(std::shared_ptr<EffectItemModel> effect);
    void moveEffect(int destRow, std::shared_ptr<EffectItemModel> effect);
    void checkMonitorPosition(int);
    void seekTimeline(int);
    /** @brief Start an MLT filter job on this clip. */
    void startFilterJob(QMap<QString, QString> &, QMap<QString, QString> &, QMap<QString, QString> &);
    /** @brief An effect was reset, trigger param reload. */
    void resetEffect(int ix);
    /** @brief Ask for creation of a group. */
    void createGroup(std::shared_ptr<EffectItemModel> effectModel);
    void unGroup(CollapsibleEffectView *);
    void createRegion(int, const QUrl &);
    void deleteGroup(const QDomDocument &);
    void importClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, const QMap<QString, QString> &keyframes = QMap<QString, QString>());
    void switchHeight(std::shared_ptr<EffectItemModel> model, int height);
    void startDrag(const QPixmap, const QString assetId, ObjectId sourceObject, int row, bool singleTargetDrag);
    void activateEffect(int row);
    void showEffectZone(ObjectId id, QPair<int, int> inOut, bool checked);
    void refresh();
};
