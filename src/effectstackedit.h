/***************************************************************************
                          effecstackedit.h  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EFFECTSTACKEDIT_H
#define EFFECTSTACKEDIT_H

#include "definitions.h"
#include "timecode.h"
#include "keyframeedit.h"

#include <QWidget>
#include <QDomElement>
#include <QVBoxLayout>
#include <QList>
#include <QMap>
#include <QScrollArea>

enum WIPE_DIRECTON { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, CENTER = 4 };

struct wipeInfo {
    WIPE_DIRECTON start;
    WIPE_DIRECTON end;
    int startTransparency;
    int endTransparency;
};

class QFrame;

class EffectStackEdit : public QScrollArea
{
    Q_OBJECT
public:
    EffectStackEdit(QWidget *parent);
    ~EffectStackEdit();
    void updateProjectFormat(MltVideoProfile profile, Timecode t);
    static QMap<QString, QImage> iconCache;
    void updateParameter(const QString &name, const QString &value);
    void setFrameSize(QPoint p);
    void updateTimecodeFormat();

private:
    void clearAllItems();
    QVBoxLayout *m_vbox;
    QList<QWidget*> m_uiItems;
    QWidget *m_baseWidget;
    QDomElement m_params;
    QMap<QString, QWidget*> m_valueItems;
    void createSliderItem(const QString& name, int val , int min, int max, const QString);
    wipeInfo getWipeInfo(QString value);
    QString getWipeString(wipeInfo info);
    MltVideoProfile m_profile;
    Timecode m_timecode;
    int m_in;
    int m_out;
    QPoint m_frameSize;
    KeyframeEdit *m_keyframeEditor;

public slots:
    /** \brief Called when an effect is selected, builds the UI for this effect */
    void transferParamDesc(const QDomElement, int, int , int);
    void slotSliderMoved(int);
    /** \brief Called whenever(?) some parameter is changed in the gui.
     *
     * Transfers all Dynamic gui parameter settings into m_params(??) */
    void collectAllParameters();

signals:
    void parameterChanged(const QDomElement, const QDomElement);
    void seekTimeline(int);
    void displayMessage(const QString&, int);
};

#endif
