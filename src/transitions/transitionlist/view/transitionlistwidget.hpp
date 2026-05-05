/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/assetlist/view/assetlistwidget.hpp"
#include "kdenlivesettings.h"

class TransitionIconDelegate;
class QProcess;

/** @class TransitionListWidget
    @brief This class is a widget that display the list of available effects
 */
class TransitionListWidget : public AssetListWidget
{
    Q_OBJECT

public:
    TransitionListWidget(QAction *includeList, QAction *tenBit, QWidget *parent = Q_NULLPTR);
    ~TransitionListWidget() override;
    bool isEffect() const override { return false; }
    void setFilterType(const QString &type) override;
    bool isAudio(const QString &assetId) const override;
    /** @brief Return mime type used for drag and drop. It will be kdenlive/composition
     or kdenlive/transition*/
    QString getMimeType(const QString &assetId) const override;
    void refreshLumas();
    void reloadCustomEffectIx(const QModelIndex &path) override;
    void reloadTemplates() override;
    void editCustomAsset(const QModelIndex &index) override;
    void exportCustomEffect(const QModelIndex &index) override;
    void switchTenBitFilter() override;

public Q_SLOTS:
    void reloadCustomEffect(const QString &path) override;

    /**
     * @brief Generate preview GIFs for transitions
     */
    void generatePreviews();
    void switchSplitter(bool enable) override;

private Q_SLOTS:
    void showLumas();
    void updateTransitionInfo(const QModelIndex &current, const QModelIndex &previous);
    void iconViewEntered(const QModelIndex &ix);
    void iconViewExited();
    void previewDone(int exitCode, QProcess::ExitStatus exitStatus);
    /**
     * @brief Start preview generation if it does not exist yet
     */
    void checkPreviews(bool force = false);

private:
    TransitionIconDelegate *m_iconDelegate;
    QMetaObject::Connection m_hoverAnimationConnection;
    QString m_hoveredTransition;
    std::unique_ptr<QProcess> m_previewProcess;
    QAction *m_generatePreviewAction{nullptr};
};
