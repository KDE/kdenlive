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


#include <QWidget>
#include <QDomElement>
#include <QItemDelegate>
#include <QAbstractItemView>
#include <QSpinBox>


#include "ui_keyframeeditor_ui.h"
#include "definitions.h"
#include "keyframehelper.h"

class KeyItemDelegate: public QItemDelegate
{
    Q_OBJECT
public:
    KeyItemDelegate(int min, int max, QAbstractItemView* parent = 0): QItemDelegate(parent), m_min(min), m_max(max) {
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        if (index.column() == 1) {
            QSpinBox *spin = new QSpinBox(parent);
            connect(spin, SIGNAL(valueChanged(int)), this, SLOT(commitEditorData()));
            connect(spin, SIGNAL(editingFinished()), this, SLOT(commitAndCloseEditor()));
            return spin;
        } else {
            return QItemDelegate::createEditor(parent, option, index);
        }
    }


    void setEditorData(QWidget *editor, const QModelIndex &index) const {
        if (index.column() == 1) {
            QSpinBox *spin = qobject_cast< QSpinBox* >(editor);
            spin->setRange(m_min, m_max);
            spin->setValue(index.model()->data(index).toInt());
        } else {
            QItemDelegate::setEditorData(editor, index);
        }
    }

private slots:
    void commitAndCloseEditor() {
        QSpinBox *spin = qobject_cast< QSpinBox* >(sender());
        emit closeEditor(spin);
    }

    void commitEditorData() {
        QSpinBox *spin = qobject_cast< QSpinBox* >(sender());
        emit commitData(spin);
    }

private:
    int m_min;
    int m_max;
};

class KeyframeEdit : public QWidget, public Ui::KeyframeEditor_UI
{
    Q_OBJECT
public:
    explicit KeyframeEdit(QDomElement e, int minFrame, int maxFrame, int minVal, int maxVal, Timecode tc, int active_keyframe, QWidget* parent = 0);
    virtual ~KeyframeEdit();
    void setupParam();
    void addParameter(QDomElement e);
    const QString getValue(const QString &name);
    /** @brief Updates the timecode display according to settings (frame number or hh:mm:ss:ff) */
    void updateTimecodeFormat();

private:
    QList <QDomElement> m_params;
    int m_min;
    int m_max;
    int m_minVal;
    int m_maxVal;
    Timecode m_timecode;
    int m_previousPos;
    //KeyItemDelegate *m_delegate;
    void generateAllParams();
    QGridLayout *m_slidersLayout;
    int m_active_keyframe;

    /** @brief Gets the position of a keyframe from the table.
    * @param row Row of the keyframe in the table */
    int getPos(int row);
    /** @brief Converts a frame value to timecode considering the frames vs. HH:MM:SS:FF setting.
    * @return timecode */
    QString getPosString(int pos);

private slots:
    void slotDeleteKeyframe();
    void slotAddKeyframe();
    void slotGenerateParams(int row, int column);
    void slotAdjustKeyframeInfo(bool seek = true);
    void slotAdjustKeyframePos(int value);
    void slotAdjustKeyframeValue(int value);
    /** @brief Turns the seek to keyframe position setting on/off.
    * @param state State of the associated checkbox */
    void slotSetSeeking(int state);
    //void slotSaveCurrentParam(QTreeWidgetItem *item, int column);

signals:
    void parameterChanged();
    void seekToPos(int);
};

#endif
