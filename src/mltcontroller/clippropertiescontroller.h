/*
SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2025 Julius Künzel <julius.kuenzel@kde.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "widgets/elidedfilelinklabel.h"

#include <KMessageWidget>

#include <QFutureWatcher>
#include <QLabel>
#include <QString>
#include <QStyledItemDelegate>
#include <QTreeWidget>

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
class QSortFilterProxyModel;
class QHBoxLayout;
class QVBoxLayout;

class PropertiesDelegate : public QStyledItemDelegate
{

public:
    PropertiesDelegate(QObject *parent);

protected:
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class AnalysisTree : public QTreeWidget
{
public:
    explicit AnalysisTree(QWidget *parent = nullptr);

protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &list) const override;
};

/** @class ClipPropertiesController
    @brief This class creates the widgets allowing to edit clip properties
 */
class ClipPropertiesController : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructor.
     * @param clipName The clip's name
     * @param controller The clip's controller
     * @param parent The widget where our infos will be displayed
     */
    explicit ClipPropertiesController(const QString &clipName, ClipController *controller, QWidget *parent);
    ~ClipPropertiesController() override;
    void activatePage(int ix);

public Q_SLOTS:
    void slotReloadProperties();
    void slotFillMeta(QTreeWidget *tree);
    void slotFillAnalysisData();
    void updateStreamInfo(int streamIndex);

private Q_SLOTS:
    void slotColorModified(const QColor &newcolor);
    void slotTimecodeChanged(int value);
    void slotDurationChanged(int duration);
    void slotEnableForce(int state);
    void slotDeleteAnalysis();
    void slotSaveAnalysis();
    void slotLoadAnalysis();
    void slotAspectValueChanged(int);
    void slotTextChanged();
    void updateTab(int ix);
    void extractInfo(const QString &url);
    void addMetadata(const QMap<QString, QString> meta);

private:
    ClipController *m_controller;
    QTabWidget *m_tabWidget;
    ElidedFileLinkLabel *m_clipLabel;
    QString m_id;
    QFutureWatcher<void> m_watcher;
    QFuture<void> m_extractJob;
    bool m_closing{false};
    ClipType::ProducerType m_type;
    /** @brief: the properties of the active producer (can be a proxy) */
    std::shared_ptr<Mlt::Properties> m_properties;
    /** @brief: the properties of the original source producer (cannot be a proxy) */
    std::shared_ptr<Mlt::Properties> m_sourceProperties;
    QMap<QString, QString> m_originalProperties;
    QMap<QString, QString> m_clipProperties;
    QList<int> m_videoStreams;
    QTreeWidget *m_propertiesTree;
    QWidget *m_propertiesPage;
    QWidget *m_metaPage;
    QWidget *m_analysisPage;
    QComboBox *m_audioStream;
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
    KMessageWidget *m_warningMessage;
    /** @brief The selected audio stream. */
    int m_activeAudioStreams;
    QList<QStringList> getVideoProperties(int streamIndex);
    QList<QStringList> getAudioProperties(int streamIndex);
    void fillProperties();
    /** @brief Add/remove icon beside audio stream to indicate effects. */
    void updateStreamIcon(int row, int streamIndex);

    void constructFileInfoPage();
    QWidget *constructPropertiesPage();
    QWidget *constructAudioPropertiesPage();
    void constructMetadataPage();
    void constructAnalysisPage();

    QHBoxLayout *comboboxProperty(const QString &label, const QString &propertyName, const QMap<QString, int> &options, const QString &defaultValue = {});
    QHBoxLayout *doubleSpinboxProperty(const QString &label, const QString &propertyName, double maxValue, double defaultValue = 0);
    QHBoxLayout *proxyProperty(const QString &label, const QString &propertyName);
    QHBoxLayout *durationProperty(const QString &label, const QString &propertyName);
    QHBoxLayout *timecodeProperty(const QString &label, const QString &propertyName);
    QHBoxLayout *aspectRatioProperty(const QString &label);
    QVBoxLayout *textProperty(const QString &label, const QString &propertyName);

    QMap<QString, QString> getMetadateMagicLantern();
    QMap<QString, QString> getMetadataExif();

Q_SIGNALS:
    void updateClipProperties(const QString &, const QMap<QString, QString> &, const QMap<QString, QString> &);
    void colorModified(const QColor &);
    /** @brief Duration value needs to be reloaded. */
    void durationModified(int);
    /** @brief Timecode value needs to be reloaded. */
    void timecodeModified(int);
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
