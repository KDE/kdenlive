/*  This file is part of Kdenlive. See www.kdenlive.org.
    SPDX-FileCopyrightText: 2024 Chengkun Chen <serix2004@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "ui_editsubstyle_ui.h"
#include <QDialog>
#include <mlt++/MltProducer.h>
#include <qdir.h>

class SubtitleModel;

class SubtitleStyleEdit : public QDialog, public Ui::SubtitleStyleEdit_UI
{
    Q_OBJECT

public:
    static const SubtitleStyle getStyle(QWidget *parent, const SubtitleStyle &style, QString &styleName, std::shared_ptr<const SubtitleModel> model,
                                        bool global, bool *ok);

private:
    explicit SubtitleStyleEdit(QWidget *parent = nullptr);
    ~SubtitleStyleEdit();

    SubtitleStyle m_style;
    bool *m_ok{nullptr};
    int m_width = 1920;
    int m_height = 1080;
    QString m_previewPath;

    void setProperties(const SubtitleStyle &style, const QString &styleName, const int previewWidth, const int previewHeight, const QString previewPath,
                       bool *ok);
    void updateProperties();

    std::unique_ptr<Mlt::Profile> m_previewProfile;
    std::unique_ptr<Mlt::Producer> m_previewProducer;
    std::unique_ptr<Mlt::Filter> m_previewFilter;
    QFile m_previewFile;
};
