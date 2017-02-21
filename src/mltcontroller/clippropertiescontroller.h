/*
Copyright (C) 2015  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CLIPPROPERTIESCONTROLLER_H
#define CLIPPROPERTIESCONTROLLER_H

#include "definitions.h"
#include "timecode.h"

#include <mlt++/Mlt.h>
#include <QString>

class ClipController;
class QMimeData;
class QTextEdit;
class QLabel;

class AnalysisTree : public QTreeWidget
{
public:
    explicit AnalysisTree(QWidget *parent = nullptr);

protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> list) const Q_DECL_OVERRIDE;
};

/**
 * @class ClipPropertiesController
 * @brief This class creates the widgets allowing to edit clip properties
 */

class ClipPropertiesController : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructor.
     * @param id The clip's id
     * @param properties The clip's properties
     * @param parent The widget where our infos will be displayed
     */
    explicit ClipPropertiesController(const Timecode &tc, ClipController *controller, QWidget *parent);
    virtual ~ClipPropertiesController();

public slots:
    void slotReloadProperties();
    void slotRefreshTimeCode();
    void slotFillMarkers();
    void slotFillMeta(QTreeWidget *tree);
    void slotFillAnalysisData();

private slots:
    void slotColorModified(QColor newcolor);
    void slotDurationChanged(int duration);
    void slotEnableForce(int state);
    void slotValueChanged(double);
    void slotSeekToMarker();
    void slotEditMarker();
    void slotDeleteMarker();
    void slotAddMarker();
    void slotLoadMarkers();
    void slotSaveMarkers();
    void slotDeleteAnalysis();
    void slotSaveAnalysis();
    void slotLoadAnalysis();
    void slotAspectValueChanged(int);
    void slotComboValueChanged();
    void slotValueChanged(int value);
    void slotTextChanged();
    void updateTab(int ix);

private:
    ClipController *m_controller;
    QTabWidget *m_tabWidget;
    QLabel *m_clipLabel;
    Timecode m_tc;
    QString m_id;
    ClipType m_type;
    Mlt::Properties m_properties;
    QMap<QString, QString> m_originalProperties;
    QMap<QString, QString> m_clipProperties;
    QTreeWidget *m_propertiesTree;
    QWidget *m_forcePage;
    QWidget *m_propertiesPage;
    QWidget *m_markersPage;
    QWidget *m_metaPage;
    QWidget *m_analysisPage;
    QTreeWidget *m_markerTree;
    AnalysisTree *m_analysisTree;
    QTextEdit *m_textEdit;
    void fillProperties();

signals:
    void updateClipProperties(const QString &, const QMap<QString, QString> &, const QMap<QString, QString> &);
    void modified(const QColor &);
    void modified(int);
    void updateTimeCodeFormat();
    /** @brief Seek clip monitor to a frame. */
    void seekToFrame(int);
    /** @brief Edit clip markers. */
    void addMarkers(const QString &, const QList<CommentedTime> &);
    void loadMarkers(const QString &);
    void saveMarkers(const QString &);
    void editAnalysis(const QString &id, const QString &name, const QString &value);
    void editClip();
};

#endif
