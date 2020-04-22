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
    /** @brief Collaps / expand current effect
     */
    void switchCollapsed();

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

    /** @brief the frame size of the original clip this effect is applied on
     */
    QSize m_sourceFrameSize;
    const QString getStyleSheet();

private slots:
    void refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
    void slotAdjustDelegate(const std::shared_ptr<EffectItemModel> &effectModel, int height);
    void slotStartDrag(const QPixmap &pix, const std::shared_ptr<EffectItemModel> &effectModel);
    void slotActivateEffect(const std::shared_ptr<EffectItemModel> &effectModel);
    void loadEffects();
    void updateTreeHeight();
    void doActivateEffect(int row, QModelIndex ix, bool force = false);
    /** @brief Save current effect stack
     */
    void slotSaveStack();
    /** @brief Refresh the enabled state on widgets
     */
    void changeEnabledState();

    //    void switchBuiltStack(bool show);

signals:
    void switchCollapsedView(int row);
    void seekToPos(int);
    void reloadEffect(const QString &path);
    void updateEnabledState();
    void removeCurrentEffect();
    void blockWheenEvent(bool block);
    void checkScrollBar();
};

#endif
