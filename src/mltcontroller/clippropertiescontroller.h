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

#include <QString>
#include <QTreeWidget>
#include <QLabel>

#include <mlt++/Mlt.h>

class ClipController;
class QMimeData;
class QTextEdit;
class QComboBox;
class QListWidget;
class QGroupBox;
class QCheckBox;
class QButtonGroup;
class QSpinBox;

class ElidedLinkLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ElidedLinkLabel(QWidget *parent = nullptr);
    void setLabelText(const QString &text, const QString &link);
    void updateText(int width);
    int currentWidth() const;

private:
    QString m_text;
    QString m_link;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

class AnalysisTree : public QTreeWidget
{
public:
    explicit AnalysisTree(QWidget *parent = nullptr);

protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> list) const override;
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
    explicit ClipPropertiesController(ClipController *controller, QWidget *parent);
    ~ClipPropertiesController() override;
    void activatePage(int ix);

public slots:
    void slotReloadProperties();
    void slotRefreshTimeCode();
    void slotFillMeta(QTreeWidget *tree);
    void slotFillAnalysisData();
    void slotDeleteSelectedMarkers();
    void slotSelectAllMarkers();
    void updateStreamInfo(int streamIndex);

private slots:
    void slotColorModified(const QColor &newcolor);
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
    ElidedLinkLabel *m_clipLabel;
    Timecode m_tc;
    QString m_id;
    ClipType::ProducerType m_type;
    /** @brief: the properties of the active producer (can be a proxy) */
    std::shared_ptr<Mlt::Properties> m_properties;
    /** @brief: the properties of the original source producer (cannot be a proxy) */
    Mlt::Properties m_sourceProperties;
    QMap<QString, QString> m_originalProperties;
    QMap<QString, QString> m_clipProperties;
    QList<int> m_videoStreams;
    QTreeWidget *m_propertiesTree;
    QWidget *m_propertiesPage;
    QWidget *m_markersPage;
    QWidget *m_metaPage;
    QWidget *m_analysisPage;
    QComboBox *m_audioStream;
    QTreeView *m_markerTree;
    AnalysisTree *m_analysisTree;
    QTextEdit *m_textEdit;
    QListWidget *m_audioStreamsView;
    QGroupBox *m_audioEffectGroup;
    QCheckBox *m_swapChannels;
    QCheckBox *m_normalize;
    QButtonGroup *m_copyChannelGroup;
    QCheckBox *m_copyChannel1;
    QCheckBox *m_copyChannel2;
    QSpinBox *m_gain;
    /** @brief The selected audio stream. */
    int m_activeAudioStreams;
    void fillProperties();
    /** @brief Add/remove icon beside audio stream to indicate effects. */
    void updateStreamIcon(int row, int streamIndex);

signals:
    void updateClipProperties(const QString &, const QMap<QString, QString> &, const QMap<QString, QString> &);
    void modified(const QColor &);
    void modified(int);
    void updateTimeCodeFormat();
    /** @brief Seek clip monitor to a frame. */
    void seekToFrame(int);
    void editAnalysis(const QString &id, const QString &name, const QString &value);
    void editClip();
    void requestProxy(bool doProxy);
    void proxyModified(const QString &);
    void deleteProxy();
    void enableProxy(bool);
};

#endif
