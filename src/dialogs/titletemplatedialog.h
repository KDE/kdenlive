/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef TITLETEMPLATEDIALOG_H
#define TITLETEMPLATEDIALOG_H

#include "ui_templateclip_ui.h"

class QResizeEvent;

class TitleTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TitleTemplateDialog(const QString &folder, QWidget *parent = nullptr);
    QString selectedTemplate() const;
    QString selectedText() const;

protected:
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

private:
    Ui::TemplateClip_UI m_view;

private slots:
    void updatePreview();
};

#endif
