/***************************************************************************
                          titlewidget.h  -  description
                             -------------------
    begin                : Feb 28 2008
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

#ifndef STOPMOTION_H
#define STOPMOTION_H

#include "ui_stopmotion_ui.h"
#include "definitions.h"

#include <QUrl>
#include <QLabel>
#include <QFuture>
#include <QTimer>
#include "monitor/abstractmonitor.h"

class MltDeviceCapture;
class MonitorManager;
class MltVideoProfile;

class MyLabel : public QLabel
{
    Q_OBJECT
public:
    explicit MyLabel(QWidget *parent = nullptr);
    void setImage(const QImage &img);

protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *) Q_DECL_OVERRIDE;

private:
    QImage m_img;

signals:
    /** @brief Seek to next or previous frame.
     *  @param forward set to true to go to next frame, fals to go to previous frame */
    void seek(bool forward);

    /** @brief Switch to live view. */
    void switchToLive();
};

class StopmotionMonitor : public AbstractMonitor
{
    Q_OBJECT
public:
    StopmotionMonitor(MonitorManager *manager, QWidget *parent);
    ~StopmotionMonitor();
    AbstractRender *abstractRender() Q_DECL_OVERRIDE;
    Kdenlive::MonitorId id() const;
    void setRender(MltDeviceCapture *render);
    void mute(bool, bool) Q_DECL_OVERRIDE;

private:
    MltDeviceCapture *m_captureDevice;

public slots:
    void stop() Q_DECL_OVERRIDE;
    void start() Q_DECL_OVERRIDE;
    void slotPlay() Q_DECL_OVERRIDE;
    void slotMouseSeek(int eventDelta, int modifiers) Q_DECL_OVERRIDE;
    void slotSwitchFullScreen(bool minimizeOnly = false) Q_DECL_OVERRIDE;

signals:
    void stopCapture();
};

class StopmotionWidget : public QDialog, public Ui::Stopmotion_UI
{
    Q_OBJECT

public:

    /** @brief Build the stopmotion dialog.
     * @param projectFolder The current project folder, where captured files will be stored.
     * @param actions The actions for this widget that can have a keyboard shortcut.
     * @param parent (optional) parent widget */
    StopmotionWidget(MonitorManager *manager, const QUrl &projectFolder, const QList< QAction * > &actions, QWidget *parent = nullptr);
    virtual ~StopmotionWidget();

protected:
    void closeEvent(QCloseEvent *e) Q_DECL_OVERRIDE;

private:
    /** @brief Current project folder (where the captured frames will be saved). */
    QUrl m_projectFolder;

    /** @brief Capture holder that will handle all video operation. */
    MltDeviceCapture *m_captureDevice;

    /** @brief Holds the name of the current sequence.
     * Files will be saved in project folder with name: sequence001.png */
    QString m_sequenceName;

    /** @brief Holds the frame number of the current sequence. */
    int m_sequenceFrame;

    QAction *m_captureAction;

    /** @brief Holds the index of the frame to be displayed in the frame preview mode. */
    int m_animatedIndex;

    /** @brief Find all stopmotion sequences in current project folder. */
    void parseExistingSequences();

    /** @brief This widget will hold the frame preview. */
    MyLabel *m_frame_preview;

    /** @brief The list of files in the sequence to create thumbnails. */
    QStringList m_filesList;

    /** @brief Holds the state of the threaded thumbnail generation. */
    QFuture<void> m_future;

    /** @brief The action triggering display of last frame over current live video feed. */
    QAction *m_showOverlay;

    /** @brief The list of all frames path. */
    QStringList m_animationList;

    /** @brief Tells if we are in animation (playback) mode. */
    bool m_animate;

    /** @brief Timer for interval capture. */
    QTimer m_intervalTimer;

    MonitorManager *m_manager;

    /** @brief The monitor is used to control the v4l capture device from the monitormanager class. */
    StopmotionMonitor *m_monitor;

    /** @brief Create the XML playlist. */
    const QString createProducer(const MltVideoProfile &profile, const QString &service, const QString &resource);

    /** @brief A new frame arrived, reload overlay. */
    void reloadOverlay();

    /** @brief Holds the index of the effect to be applied to the video feed. */
    int m_effectIndex;

public slots:
    /** @brief Display the live feed from capture device.
     @param isOn enable or disable the feature */
    void slotLive(bool isOn = true);
    void slotStopCapture();

private slots:

    /** @brief Display the last captured frame over current live feed.
     @param isOn enable or disable the feature */
    void slotShowOverlay(bool isOn);

    /** @brief Display the last captured frame over current live feed. */
    void slotUpdateOverlay();

    /** @brief User changed the capture name. */
    void sequenceNameChanged(const QString &name);

    /** @brief Grab a frame from current capture feed. */
    void slotCaptureFrame();

    /** @brief Display a previous frame in monitor. */
    void slotShowFrame(const QString &path);

    /** @brief Get full path for a frame in the sequence.
     *  @param ix the frame number.
     *  @param seqName (optional) the name of the sequence. */
    QString getPathForFrame(int ix, QString seqName = QString());

    /** @brief Add sequence to current project. */
    void slotAddSequence();

    /** @brief Display selected fram in monitor. */
    void slotShowSelectedFrame();

    /** @brief Start animation preview mode. */
    void slotPlayPreview(bool animate);

    /** @brief Simulate animation. */
    void slotAnimate();

    /** @brief Seek to previous or next captured frame.
     *  @param forward set to true for next frame, false for previous one. */
    void slotSeekFrame(bool forward);

    /** @brief Display warning / error message from capture backend. */
    void slotGotHDMIMessage(const QString &message);

    /** @brief Create thumbnails for existing sequence frames. */
    void slotCreateThumbs(const QImage &img, int ix);

    /** @brief Prepare thumbnails creation. */
    void slotPrepareThumbs();

    /** @brief Called when user switches the video capture backend. */
    void slotUpdateDeviceHandler();

    /** @brief Show / hide sequence thumbnails. */
    void slotShowThumbs(bool show);

    /** @brief Show the config dialog */
    void slotConfigure();

    /** @brief Prepare to crete thumb for newly captured frame. */
    void slotNewThumb(const QString &path);

    /** @brief Set the effect to be applied to overlay frame. */
    void slotUpdateOverlayEffect(QAction *act);

    /** @brief Switch between live view / currently selected frame. */
    void slotSwitchLive();

    /** @brief Delete current frame from disk. */
    void slotRemoveFrame();

    /** @brief Enable / disable frame analysis (in color scopes). */
    void slotSwitchAnalyse(bool isOn);

    /** @brief Enable / disable horizontal mirror effect. */
    void slotSwitchMirror(bool isOn);

    /** @brief Send a notification a few seconds before capturing. */
    void slotPreNotify();

signals:
    /** @brief Ask to add sequence to current project. */
    void addOrUpdateSequence(const QString &);

    void doCreateThumbs(const QImage &, int);
    void gotFrame(const QImage &);
};

#endif
