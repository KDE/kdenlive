/*
    SPDX-FileCopyrightText: 2022 Gary Wang <wzc782970009@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "exportguidesdialog.h"

#include "bin/model/markerlistmodel.hpp"
#include "core.h"

#include "kdenlive_debug.h"
#include <KMessageWidget>
#include <QFontDatabase>
#include <QPushButton>
#include <QClipboard>
#include <QTime>

#include "klocalizedstring.h"

#define YT_FORMAT "{{timecode}} {{comment}}"

ExportGuidesDialog::ExportGuidesDialog(const MarkerListModel *model, const GenTime duration, QWidget *parent)
    : QDialog(parent)
    , m_markerListModel(model)
    , m_projectDuration(duration)
{
//    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18n("Export guides as chapters description"));

    const QString defaultFormat(YT_FORMAT);
    formatEdit->setText(defaultFormat);

    static std::array<QColor, 9> markerTypes = model->markerTypes;
    QPixmap pixmap(32,32);
    for (uint i = 1; i <= markerTypes.size(); ++i) {
        pixmap.fill(markerTypes[size_t(i - 1)]);
        QIcon colorIcon(pixmap);
        markerTypeComboBox->addItem(colorIcon, i18n("Category %1", i - 1));
    }
    markerTypeComboBox->setCurrentIndex(0);
    messageWidget->setText(i18n(
        "If you are using the exported text for YouTube, you might want to check:\n"
        "1. The start time of 00:00 must have a chapter.\n"
        "2. There must be at least three timestamps in ascending order.\n"
        "3. The minimum length for video chapters is 10 seconds."
    ));
    messageWidget->setVisible(false);

    updateContentByModel(defaultFormat, markerTypeComboBox->currentIndex() - 1);

    QPushButton * btn = buttonBox->addButton(i18n("Copy to Clipboard"), QDialogButtonBox::ActionRole);
    btn->setIcon(QIcon::fromTheme("edit-copy"));

    connect(markerTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int newIndex) {
        updateContentByModel(formatEdit->text(), newIndex - 1);
    });

    connect(formatEdit, &QLineEdit::textEdited, this, [this]() {
        updateContentByModel(formatEdit->text(), markerTypeComboBox->currentIndex() - 1);
    });

    connect(btn, &QAbstractButton::clicked, this, [this]() {
         QClipboard *clipboard = QGuiApplication::clipboard();
         clipboard->setText(this->generatedContent->toPlainText());
    });

    adjustSize();
}

ExportGuidesDialog::~ExportGuidesDialog()
{
}

void ExportGuidesDialog::updateContentByModel(const QString & format, int markerIndex) const
{
    QStringList chapterTexts;
    QList<CommentedTime> markers(m_markerListModel->getAllMarkers(markerIndex));
    bool needCheck = format == YT_FORMAT;
    bool needShowInfoMsg = false;

    const int markerCount = markers.length();
    for (int i = 0; i < markers.length(); i++) {
        const CommentedTime &currentMarker = markers.at(i);
        const GenTime &nextGenTime = markerCount - 1 == i ? m_projectDuration : markers.at(i + 1).time();

        QString line(format);
        QTime currentTime = QTime(0,0).addMSecs(currentMarker.time().ms());
        QTime nextTime = QTime(0,0).addMSecs(nextGenTime.ms());

        if (i == 0 && needCheck && currentTime.msec() != 0) {
            needShowInfoMsg = true;
        }

        if (needCheck && currentTime.secsTo(nextTime) < 10) {
            needShowInfoMsg = true;
        }

        line.replace("{{index}}", QString::number(i + 1));
        line.replace("{{timecode}}", currentTime.hour() <= 0 ? currentTime.toString("mm:ss") : currentTime.toString("h:mm:ss"));
        line.replace("{{nexttimecode}}", nextTime.hour() <= 0 ? nextTime.toString("mm:ss") : nextTime.toString("h:mm:ss"));
        line.replace("{{frame}}", QString::number(currentMarker.time().frames(pCore->getCurrentFps())));
        line.replace("{{comment}}", currentMarker.comment());
        chapterTexts.append(line);
    }

    generatedContent->setPlainText(chapterTexts.join('\n'));

    if (needCheck && markerCount < 3) {
        needShowInfoMsg = true;
    }

    messageWidget->setVisible(needShowInfoMsg);
}

#undef YT_FORMAT
