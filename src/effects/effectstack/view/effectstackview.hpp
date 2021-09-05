/*
 *   SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef EFFECTSTACKVIEW_H
#define EFFECTSTACKVIEW_H

#include "definitions.h"
#include <QMutex>
#include <QStyledItemDelegate>
#include <QWidget>
#include <QTimer>
#include <memory>

class QVBoxLayout;
class QTreeView;
class CollapsibleEffectView;
class AssetParameterModel;
class EffectStackModel;
class EffectItemModel;
class AssetIconProvider;
class BuiltStack;
class AssetPanel;

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
    QMap<QModelIndex, int> m_height;
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

public slots:
    /** @brief Save current effect stack
     */
    void slotSaveStack();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QMutex m_mutex;
    QVBoxLayout *m_lay;
    // BuiltStack *m_builtStack;
    QTreeView *m_effectsTree;
    std::shared_ptr<EffectStackModel> m_model;
    std::vector<CollapsibleEffectView *> m_widgets;
    AssetIconProvider *m_thumbnailer;
    QTimer m_scrollTimer;
    QTimer m_timerHeight;

    /** @brief the frame size of the original clip this effect is applied on
     */
    QSize m_sourceFrameSize;
    const QString getStyleSheet();

private slots:
    void refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
    void slotAdjustDelegate(const std::shared_ptr<EffectItemModel> &effectModel, int height);
    void slotStartDrag(const QPixmap &pix, const std::shared_ptr<EffectItemModel> &effectModel);
    void loadEffects();
    void updateTreeHeight();
    void slotFocusEffect();
    /** @brief Refresh the enabled state on widgets
     */
    void changeEnabledState();
    /** @brief Activate an effect in the view
     */
    void activateEffect(QModelIndex ix, bool active);

    //    void switchBuiltStack(bool show);

signals:
    void switchCollapsedView(int row);
    void seekToPos(int);
    void reloadEffect(const QString &path);
    void updateEnabledState();
    void removeCurrentEffect();
    void blockWheelEvent(bool block);
    void checkScrollBar();
    void scrollView(QRect);
};

#endif
