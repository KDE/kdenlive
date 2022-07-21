/*
    SPDX-FileCopyrightText: 2022 Gary Wang <wzc782970009@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "exportguidesdialog.h"

#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "kdenlivesettings.h"

#include "kdenlive_debug.h"
#include <KMessageWidget>
#include <QClipboard>
#include <QDateTimeEdit>
#include <QFontDatabase>
#include <QPushButton>
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

    // We should setup TimecodeDisplay since it requires a proper Timecode
    offsetTimeSpinbox->setTimecode(Timecode(Timecode::HH_MM_SS_FF, pCore->getCurrentFps()));

    const QString defaultFormat(YT_FORMAT);
    formatEdit->setText(KdenliveSettings::exportGuidesFormat().isEmpty() ? defaultFormat : KdenliveSettings::exportGuidesFormat());

    static std::array<QColor, 9> markerTypes = model->markerTypes;
    QPixmap pixmap(32, 32);
    for (uint i = 1; i <= markerTypes.size(); ++i) {
        pixmap.fill(markerTypes[size_t(i - 1)]);
        QIcon colorIcon(pixmap);
        markerTypeComboBox->addItem(colorIcon, i18n("Category %1", i - 1));
    }
    markerTypeComboBox->setCurrentIndex(0);
    messageWidget->setText(i18n("If you are using the exported text for YouTube, you might want to check:\n"
                                "1. The start time of 00:00 must have a chapter.\n"
                                "2. There must be at least three timestamps in ascending order.\n"
                                "3. The minimum length for video chapters is 10 seconds."));
    messageWidget->setVisible(false);

    updateContentByModel();

    QPushButton *btn = buttonBox->addButton(i18n("Copy to Clipboard"), QDialogButtonBox::ActionRole);
    btn->setIcon(QIcon::fromTheme("edit-copy"));

    connect(markerTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { updateContentByModel(); });

    connect(offsetTimeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int newIndex) {
        offsetTimeSpinbox->setEnabled(newIndex != 0);
        updateContentByModel();
    });

    connect(offsetTimeSpinbox, &TimecodeDisplay::timeCodeUpdated, this, [this]() { updateContentByModel(); });

    connect(formatEdit, &QLineEdit::textEdited, this, [this]() { updateContentByModel(); });

    connect(btn, &QAbstractButton::clicked, this, [this]() {
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setText(this->generatedContent->toPlainText());
    });

    connect(buttonReset, &QAbstractButton::clicked, [this, defaultFormat]() {
        formatEdit->setText(defaultFormat);
        updateContentByModel();
    });

    adjustSize();
}

ExportGuidesDialog::~ExportGuidesDialog()
{
    KdenliveSettings::setExportGuidesFormat(formatEdit->text());
}

GenTime ExportGuidesDialog::offsetTime() const
{
    switch (offsetTimeComboBox->currentIndex()) {
    case 1: // Add
        return offsetTimeSpinbox->gentime();
    case 2: // Subtract
        return -offsetTimeSpinbox->gentime();
    case 0: // Disabled
    default:
        return GenTime(0);
    }
}

QString chapterTimeStringFromMs(double timeMs)
{
    int totalSec = qAbs(timeMs / 1000);
    bool negative = timeMs < 0 && totalSec > 0; // since our minimal unit is second.
    int hour = totalSec / 3600;
    int min = totalSec % 3600 / 60;
    int sec = totalSec % 3600 % 60;
    if (hour == 0) {
        return QString::asprintf("%s%d:%02d", negative ? "-" : "", min, sec);
    } else {
        return QString::asprintf("%s%d:%02d:%02d", negative ? "-" : "", hour, min, sec);
    }
}

void ExportGuidesDialog::updateContentByModel() const
{
    const QString format(formatEdit->text());
    const int markerIndex = markerTypeComboBox->currentIndex() - 1;
    const GenTime offset(offsetTime());

    QStringList chapterTexts;
    QList<CommentedTime> markers(m_markerListModel->getAllMarkers(markerIndex));
    bool needCheck = format == YT_FORMAT;
    bool needShowInfoMsg = false;

    const int markerCount = markers.length();
    const double currentFps = pCore->getCurrentFps();
    for (int i = 0; i < markers.length(); i++) {
        const CommentedTime &currentMarker = markers.at(i);
        const GenTime &nextGenTime = markerCount - 1 == i ? m_projectDuration : markers.at(i + 1).time();

        QString line(format);
        GenTime currentTime = currentMarker.time() + offset;
        GenTime nextTime = nextGenTime + offset;

        if (i == 0 && needCheck && !qFuzzyCompare(currentTime.seconds(), 0)) {
            needShowInfoMsg = true;
        }

        if (needCheck && (nextTime.seconds() - currentTime.seconds()) < 10) {
            needShowInfoMsg = true;
        }

        line.replace("{{index}}", QString::number(i + 1));
        line.replace("{{realtimecode}}", pCore->timecode().getDisplayTimecode(currentTime, false));
        line.replace("{{timecode}}", chapterTimeStringFromMs(currentTime.ms()));
        line.replace("{{nexttimecode}}", chapterTimeStringFromMs(nextTime.ms()));
        line.replace("{{frame}}", QString::number(currentTime.frames(currentFps)));
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
