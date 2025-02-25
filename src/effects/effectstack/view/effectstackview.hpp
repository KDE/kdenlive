/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QMutex>
#include <QReadWriteLock>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QWidget>
#include <memory>

class QVBoxLayout;
class QTreeView;
class CollapsibleEffectView;
class AssetParameterModel;
class EffectStackModel;
class EffectItemModel;
class AssetIconProvider;
class BuiltStack;
class EffectStackFilter;
class AssetPanel;
class QPushButton;

class WidgetDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit WidgetDelegate(QObject *parent = nullptr);
    void setHeight(const QModelIndex &index, int height);
    int height(const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QMap<QPersistentModelIndex, int> m_height;
    mutable QReadWriteLock m_lock;
};

class EffectStackView : public QWidget
{
    Q_OBJECT

public:
    EffectStackView(AssetPanel *parent);
    ~EffectStackView() override;
    void setModel(std::shared_ptr<EffectStackModel> model, const QSize frameSize);
    void unsetModel(bool reset = true);
    ObjectId stackOwner() const;
    /** @brief Add an effect to the current stack
     */
    bool addEffect(const QString &effectId);
    /** @brief Returns true if effectstack is empty
     */
    bool isEmpty() const;
    /** @brief Enables / disables the stack
     */
    void enableStack(bool enable);
    bool isStackEnabled() const;
    /** @brief Collapse / expand current effect
     */
    void switchCollapsed();
    /** @brief Go to next keyframe in current effect
     */
    void slotGoToKeyframe(bool next);
    void addRemoveKeyframe();
    /** @brief Used to pass a standard action like copy or paste to the effect stack widget */
    void sendStandardCommand(int command);
    /** @brief The drag pos, null if not dragging */
    QPoint dragPos;

public Q_SLOTS:
    /** @brief Save current effect stack
     */
    void slotSaveStack();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    /** @brief Install event filter so that scrolling with mouse wheel does not change parameter value. */
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    QMutex m_mutex;
    QVBoxLayout *m_lay;

    QTreeView *m_effectsTree;
    std::shared_ptr<EffectStackModel> m_model;
    std::vector<CollapsibleEffectView *> m_widgets;
    AssetIconProvider *m_thumbnailer;
    std::shared_ptr<EffectStackFilter> m_filter;
    QTimer m_scrollTimer;
    QTimer m_timerHeight;
    QPoint m_dragStart;
    bool m_dragging;
    QWidget *m_builtStack{nullptr};
    QPushButton *m_flipV{nullptr};
    QPushButton *m_flipH{nullptr};
    QPushButton *m_removeBg{nullptr};

    /** @brief the frame size of the original clip this effect is applied on
     */
    QSize m_sourceFrameSize;
    const QString getStyleSheet();
    /** @brief Start drag operation on a CollapsibleEffectView */
    void startDrag(const QPixmap pix, const QString assetId, ObjectId sourceObject, int row, bool singleTarget = false);

private Q_SLOTS:
    void refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
    void slotCollapseAllEffects(bool collapse);
    void slotAdjustDelegate(const std::shared_ptr<EffectItemModel> &effectModel, int height);
    void slotDeleteEffect(const std::shared_ptr<EffectItemModel> &effect);
    void loadEffects();
    void updateTreeHeight();
    void slotFocusEffect();
    /** @brief Refresh the enabled state on widgets
     */
    void changeEnabledState();
    /** @brief Activate an effect in the view
     */
    void activateEffect(const QModelIndex &ix, bool active);
    /** @brief Mark an effect as drop target (draw bottom highlight bar)
     */
    void setDropTargetEffect(const QModelIndex &ix, bool active);

Q_SIGNALS:
    void switchCollapsedView(int row);
    void seekToPos(int);
    void reloadEffect(const QString &path);
    void updateEnabledState();
    void removeCurrentEffect();
    void blockWheelEvent(bool block);
    void checkScrollBar();
    void scrollView(QRect);
    void checkDragScrolling();
    void updateEffectsGroupesInstances();
    void launchSam();
};
