/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "ui_maskmanage_ui.h"

#include <QDir>
#include <QMutex>
#include <QReadWriteLock>
#include <QTimer>
#include <QWidget>
#include <memory>

class ProjectClip;
class AutomaskHelper;

class MaskManager : public QWidget, public Ui::MaskManage_UI
{
    Q_OBJECT
public:
    enum MaskRoles { MASKFILE = Qt::UserRole, MASKIN, MASKOUT, MASKINCLUDEPOINTS, MASKEXCLUDEPOINTS, MASKBOXES, MASKMISSING };
    MaskManager(QWidget *parent);
    virtual ~MaskManager() override;
    void setOwner(ObjectId owner);
    bool jobRunning() const;
    /** @brief return true if a mask job is running */
    bool isLocked() const;

public Q_SLOTS:
    void launchSimpleSam();
    void abortPreviewByMonitor();
    void prepareMenu(const QPoint &pos);
    void slotOpenFolder();

private Q_SLOTS:
    void initMaskMode(bool autoAdd = false, bool editMode = false);
    void addControlPoint(int position, QSize frameSize, int xPos, int yPos, bool extend, bool exclude);
    void moveControlPoint(int ix, int position, QSize frameSize, int xPos, int yPos);
    void addControlRect(int position, QSize frameSize, const QRect rect, bool extend);
    void previewMask(bool show);
    void editMask(bool show);
    void generateMask();
    void loadMasks();
    void checkModelAvailability();
    void applyMask();
    void deleteMask();
    void importMask();

private:
    ObjectId m_owner{KdenliveObjectType::NoItem, {}};
    ObjectId m_filterOwner{KdenliveObjectType::NoItem, {}};
    AutomaskHelper *m_maskHelper;
    QPoint m_zone;
    QSize m_iconSize;
    QDir m_maskFolder;
    bool m_connected{false};
    QTimer m_paramsTimer;
    std::shared_ptr<ProjectClip> getOwnerClip();
    void exportFrames(bool autoAdd, bool editMode = false);
    void disconnectMonitor();
    void updateMasksButtons();

Q_SIGNALS:
    void maskReady();
    void progressUpdate(int progress, bool exportStep = false);
};
