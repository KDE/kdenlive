/*
SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KDirOperator>
#include <QTimer>

class KUrlNavigator;
class KUrlComboBox;
class KFileFilterCombo;

/**
 * @class MediaBrowser
 * @brief The bin widget takes care of both item model and view upon project opening.
 */
class MediaBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit MediaBrowser(QWidget *parent = nullptr);
    ~MediaBrowser() override;
    const QUrl url() const;
    void setUrl(const QUrl url);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    KDirOperator *m_op;
    KUrlNavigator *m_locationEdit;
    KFileFilterCombo *m_filterCombo;
    KUrlComboBox *m_filenameEdit;
    QTimer m_filterDelayTimer;
    void importSelection();
    void openExternalFile(const QUrl &url);

private Q_SLOTS:
    void slotFilterChanged();
    void slotLocationChanged(const QString &text);
    void slotUrlEntered(const QUrl &url);
};
