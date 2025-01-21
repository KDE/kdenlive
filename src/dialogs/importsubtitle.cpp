/*
    SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "importsubtitle.h"
#include "bin/model/subtitlemodel.hpp"
#include "core.h"

#include "kdenlive_debug.h"
#include <QFontDatabase>

#include "klocalizedstring.h"
#include <KCharsets>

ImportSubtitle::ImportSubtitle(const QString &path, QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    m_parseTimer.setSingleShot(true);
    m_parseTimer.setInterval(200);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    QStringList listCodecs = KCharsets::charsets()->descriptiveEncodingNames();
    const QString filter = QStringLiteral("*.srt *.ass *.vtt *.sbv");
    subtitle_url->setNameFilter(filter);
    for (auto &l : listCodecs) {
        codecs_list->addItem(l, KCharsets::charsets()->encodingForName(l));
    }
    info_message->setVisible(false);
    // Set UTF-8 as default codec
    int matchIndex = codecs_list->findData(QStringLiteral("UTF-8"));
    if (matchIndex > -1) {
        codecs_list->setCurrentIndex(matchIndex);
    }
    codecs_list->setToolTip(i18n("Character encoding used to save the subtitle file."));
    codecs_list->setWhatsThis(xi18nc("@info:whatsthis", "If unsure,try :<br/><b>Unicode (UTF-8)</b>."));
    caption_original_framerate->setValue(pCore->getCurrentFps());
    caption_target_framerate->setValue(pCore->getCurrentFps());

    Fun updateSub = [this]() {
        QFile srtFile(subtitle_url->url().toLocalFile());
        if (!srtFile.exists() || !srtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            info_message->setMessageType(KMessageWidget::Warning);
            info_message->setText(i18n("Cannot read file %1", srtFile.fileName()));
            info_message->animatedShow();
            return true;
        }
        QByteArray codec = KCharsets::charsets()->encodingForName(codecs_list->currentText()).toUtf8();
        QTextStream stream(&srtFile);
        std::optional<QStringConverter::Encoding> inputEncoding = QStringConverter::encodingForName(codec.data());
        if (inputEncoding) {
            stream.setEncoding(inputEncoding.value());
        }
        // else: UTF8 is the default
        text_preview->clear();
        int maxLines = 30;
        QStringList textData;
        while (maxLines > 0) {
            const QString line = stream.readLine();
            if (!line.isEmpty()) {
                textData << line;
            }
            if (stream.atEnd()) {
                break;
            }
            maxLines--;
        }
        text_preview->setPlainText(textData.join(QLatin1Char('\n')));
        return true;
    };

    Fun checkEncoding = [this, updateSub]() {
        bool ok;
        QFile srtFile(subtitle_url->url().toLocalFile());
        if (!srtFile.exists()) {
            info_message->setMessageType(KMessageWidget::Warning);
            info_message->setText(i18n("Cannot read file %1", srtFile.fileName()));
            info_message->animatedShow();
            text_preview->clear();
            return true;
        }
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
        QString guessedEncoding(SubtitleModel::guessFileEncoding(srtFile.fileName(), &ok));
        qDebug() << "Guessed subtitle encoding is" << guessedEncoding;
        if (ok == false) {
            info_message->setMessageType(KMessageWidget::Warning);
            info_message->setText(i18n("Encoding could not be guessed, using UTF-8"));
            guessedEncoding = QStringLiteral("UTF-8");
        } else {
            info_message->setMessageType(KMessageWidget::Information);
            info_message->setText(i18n("Encoding detected as %1", guessedEncoding));
        }
        info_message->animatedShow();
        int matchIndex = codecs_list->findText(KCharsets::charsets()->descriptionForEncoding(guessedEncoding));
        if (matchIndex > -1) {
            codecs_list->setCurrentIndex(matchIndex);
            updateSub();
        }
        return true;
    };

    if (!path.isEmpty()) {
        subtitle_url->setText(path);
        checkEncoding();
    }
    connect(codecs_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [updateSub]() { updateSub(); });
    connect(subtitle_url, &KUrlRequester::urlSelected, this, [this]() { m_parseTimer.start(); });
    connect(subtitle_url, &KUrlRequester::textChanged, this, [this]() { m_parseTimer.start(); });
    connect(&m_parseTimer, &QTimer::timeout, this, [this, checkEncoding]() {
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        checkEncoding();
    });
    setWindowTitle(i18n("Import Subtitle"));
}

ImportSubtitle::~ImportSubtitle() {}
