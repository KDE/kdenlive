/***************************************************************************
                          keyframeedit.h  -  description
                             -------------------
    begin                : 22 Jun 2009
    copyright            : (C) 2008 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KEYFRAMEEDIT_H
#define KEYFRAMEEDIT_H

#include "assets/view/widgets/abstractparamwidget.hpp"
#include "definitions.h"
#include "ui_keyframeeditor_ui.h"

#include <QAbstractItemView>
#include <QDomElement>
#include <QItemDelegate>
#include <QSpinBox>
#include <QWidget>

class PositionWidget;

class KeyItemDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    KeyItemDelegate(int min, int max, QAbstractItemView *parent = nullptr)
        : QItemDelegate(parent)
        , m_min(min)
        , m_max(max)
    {
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() == 1) {
            QSpinBox *spin = new QSpinBox(parent);
            connect(spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KeyItemDelegate::commitEditorData);
            connect(spin, &QAbstractSpinBox::editingFinished, this, &KeyItemDelegate::commitAndCloseEditor);
            return spin;
        } else {
            return QItemDelegate::createEditor(parent, option, index);
        }
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (index.column() == 1) {
            QSpinBox *spin = qobject_cast<QSpinBox *>(editor);
            spin->setRange(m_min, m_max);
            spin->setValue(index.model()->data(index).toInt());
        } else {
            QItemDelegate::setEditorData(editor, index);
        }
    }

private slots:
    void commitAndCloseEditor()
    {
        QSpinBox *spin = qobject_cast<QSpinBox *>(sender());
        emit closeEditor(spin);
    }

    void commitEditorData()
    {
        QSpinBox *spin = qobject_cast<QSpinBox *>(sender());
        emit commitData(spin);
    }

private:
    int m_min;
    int m_max;
};

class KeyframeEdit : public AbstractParamWidget, public Ui::KeyframeEditor_UI
{
    Q_OBJECT
public:
    explicit KeyframeEdit(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);
    virtual ~KeyframeEdit();
    virtual void addParameter(QModelIndex index, int activeKeyframe = -1);
    const QString getValue(const QString &name);
    /** @brief Updates the timecode display according to settings (frame number or hh:mm:ss:ff) */
    void updateTimecodeFormat();

    /** @brief Returns true if the parameter @param name should be shown on the clip in timeline. */
    bool isVisibleParam(const QString &name);

    /** @brief Makes the first parameter visible in timeline if no parameter is selected. */
    void checkVisibleParam();

    /** @brief Returns attribute name for returned keyframes. */
    const QString getTag() const;

public slots:

    void slotUpdateRange(int inPoint, int outPoint);
    void slotAddKeyframe(int pos = -1);

    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

protected:
    /** @brief Gets the position of a keyframe from the table.
     * @param row Row of the keyframe in the table */
    int getPos(int row);
    /** @brief Converts a frame value to timecode considering the frames vs. HH:MM:SS:FF setting.
     * @return timecode */
    QString getPosString(int pos);

    void generateAllParams();
    QList<QModelIndex> m_paramIndexes;

    int m_min;
    int m_max;
    int m_offset;

protected slots:
    void slotAdjustKeyframeInfo(bool seek = true);

private:
    QList<QDomElement> m_params;
    QGridLayout *m_slidersLayout;
    PositionWidget *m_position;
    bool m_keyframesTag;

private slots:
    void slotDeleteKeyframe();
    void slotGenerateParams(int row, int column);
    void slotAdjustKeyframePos(int value);
    void slotAdjustKeyframeValue(double value);
    /** @brief Turns the seek to keyframe position setting on/off.
     * @param seek true = seeking on */
    void slotSetSeeking(bool seek);

    /** @brief Shows the keyframe table and adds a second keyframe. */
    void slotKeyframeMode();

    /** @brief Resets all parameters of the selected keyframe to their default values. */
    void slotResetKeyframe();

    /** @brief Makes the parameter at column @param id the visible (in timeline) one. */
    void slotUpdateVisibleParameter(int id, bool update = true);
    /** @brief A row was clicked, adjust parameters. */
    void rowClicked(int newRow, int, int oldRow, int);

signals:
    void seekToPos(int);
    void showComments(bool show);
};

#endif
