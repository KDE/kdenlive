/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef CLIPPROPERTIES_H
#define CLIPPROPERTIES_H

#include "definitions.h"
#include "timecode.h"
#include "docclipbase.h"
#include "ui_clipproperties_ui.h"

#include <QStyledItemDelegate>

class PropertiesViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    PropertiesViewDelegate(QWidget *parent) : QStyledItemDelegate(parent) {
        m_height = parent->fontMetrics().height() * 1.5;
    }
    virtual QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
        return QSize(10, m_height);
    }
private:
    int m_height;
};

class ClipProperties : public QDialog
{
    Q_OBJECT

public:
    ClipProperties(DocClipBase *clip, Timecode tc, double fps, QWidget * parent = 0);
    ClipProperties(QList <DocClipBase *>cliplist, Timecode tc, QMap <QString, QString> commonproperties, QWidget * parent);
    virtual ~ClipProperties();
    QMap <QString, QString> properties();
    const QString &clipId() const;
    bool needsTimelineRefresh() const;
    bool needsTimelineReload() const;
    void disableClipId(const QString &id);

public slots:
    void slotFillMarkersList(DocClipBase *clip);
    
private slots:
    void parseFolder();
    void slotAddMarker();
    void slotEditMarker();
    void slotDeleteMarker();
    void slotCheckMaxLength();
    void slotEnableLuma(int state);
    void slotEnableLumaFile(int state);
    void slotUpdateDurationFormat(int ix);
    void slotApplyProperties();
    void slotModified();
    void slotDeleteProxy();
    void slotOpenUrl(const QString &url);
    void slotSaveMarkers();
    void slotLoadMarkers();

private:
    Ui::ClipProperties_UI m_view;
    QMap <QString, QString> m_old_props;
    DocClipBase *m_clip;
    Timecode m_tc;
    double m_fps;
    /** used to count images in slideshow clip */
    int m_count;
    /** some visual properties changed, reload thumbnails */
    bool m_clipNeedsRefresh;
    /** clip resource changed, reload it */
    bool m_clipNeedsReLoad;
    /** Frame with proxy info / delete button */
    QFrame* m_proxyContainer;

signals:
    void addMarker(const QString &, GenTime, QString);
    void deleteProxy(const QString);
    void applyNewClipProperties(const QString, QMap <QString, QString> , QMap <QString, QString> , bool, bool);
    void saveMarkers(const QString &);
    void loadMarkers(const QString &);
};


#endif

