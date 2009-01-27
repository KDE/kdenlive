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

#include <QWidget>
#include <QDomElement>
#include <QVBoxLayout>
#include <QList>
#include <QMap>

#include "definitions.h"
#include "timecode.h"

enum WIPE_DIRECTON { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, CENTER = 4 };

struct wipeInfo {
    WIPE_DIRECTON start;
    WIPE_DIRECTON end;
    int startTransparency;
    int endTransparency;
};

class QFrame;

class EffectStackEdit : public QWidget {
    Q_OBJECT
public:
    EffectStackEdit(QWidget *parent);
    ~EffectStackEdit();
    void updateProjectFormat(MltVideoProfile profile, Timecode t);
    static QMap<QString, QImage> iconCache;

private:
    void clearAllItems();
    QVBoxLayout *vbox;
    QList<QWidget*> items;
    QList<void*> uiItems;
    QDomElement params;
    QMap<QString, void*> valueItems;
    void createSliderItem(const QString& name, int val , int min, int max);
    wipeInfo getWipeInfo(QString value);
    QString getWipeString(wipeInfo info);
    MltVideoProfile m_profile;
    Timecode m_timecode;
    int m_in;
    int m_out;

public slots:
    void transferParamDesc(const QDomElement&, int , int);
    void slotSliderMoved(int);
    /** \brief Called whenever(?) some parameter is changed in the gui.  
     * 
     * Transfers all Dynamic gui parameter settings into params(??) */
    void collectAllParameters();

private slots:
    void slotSeekToPos(int);

signals:
    void parameterChanged(const QDomElement&, const QDomElement&);
    void seekTimeline(int);
};

#endif
