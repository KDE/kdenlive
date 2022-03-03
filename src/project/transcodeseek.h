/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef TRANSCODESEEK_H
#define TRANSCODESEEK_H

#include "definitions.h"
#include "ui_transcodeseekable_ui.h"

#include <QUrl>
#include <QProcess>

class TranscodeSeek : public QDialog, public Ui::TranscodeSeekable_UI
{
    Q_OBJECT

public:
    TranscodeSeek(QWidget *parent = nullptr);
    ~TranscodeSeek() override;

    void addUrl(const QString &file, const QString &id, const QString &suffix, ClipType::ProducerType type, const QString &message);
    QMap<QString,QStringList>  ids() const;
    QString params(int clipType) const;
    QString preParams() const;
    
private:
    QMap<QString, QString> m_encodeParams;

};

#endif
