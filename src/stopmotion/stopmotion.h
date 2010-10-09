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
#include "../blackmagic/capture.h"

#include <KUrl>

class StopmotionWidget : public QDialog , public Ui::Stopmotion_UI
{
    Q_OBJECT

public:

    /** @brief Build the stopmotion dialog.
     * @param projectFolder The current project folder, where captured files will be stored.
     * @param parent (optional) parent widget */
    StopmotionWidget(KUrl projectFolder, QWidget *parent = 0);
    virtual ~StopmotionWidget();


protected:


private:
  /** @brief Current project folder (where the captured frames will be saved). */
  KUrl m_projectFolder;

  /** @brief Capture holder that will handle all video operation. */
  CaptureHandler *m_bmCapture;

  /** @brief Holds the name of the current sequence.
   * Files will be saved in project folder with name: sequence001.png */
  QString m_sequenceName;

  /** @brief Holds the frame number of the current sequence. */
  int m_sequenceFrame;

  QAction *m_captureAction;
  
  /** @brief Holds the index of the frame to be displayed in the frame preview mode. */
  int m_animatedIndex;
  
private slots:
  /** @brief Display the live feed from capture device.
   @param isOn enable or disable the feature */
  void slotLive(bool isOn);

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
  void slotShowFrame(int);

  /** @brief Get full path for a frame in the sequence.
   *  @param ix the frame number.
   *  @param seqName (optional) the name of the sequence. */
  QString getPathForFrame(int ix, QString seqName = QString());

  /** @brief Add sequence to current project. */
  void slotAddSequence();

  /** @brief Update the frame list widget with newly created frame. */
  void slotUpdateFrameList(int ix = -1);
  
  /** @brief Display selected fram in monitor. */
  void slotShowSelectedFrame();

  /** @brief Start animation preview mode. */
  void slotPlayPreview();
  
  /** @brief Simulate animation. */
  void slotAnimate();

signals:
  /** @brief Ask to add sequence to current project. */
  void addOrUpdateSequence(const QString);
};

#endif
