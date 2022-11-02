/*
    SPDX-FileCopyrightText: 2022 Gary Wang <wzc782970009@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "exportguidesdialog.h"

#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"

#include "kdenlive_debug.h"
#include <KMessageWidget>
#include <QAction>
#include <QClipboard>
#include <QDateTimeEdit>
#include <QFileDialog>
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
    categoryChooser->setMarkerModel(m_markerListModel);
    messageWidget->setText(i18n("If you are using the exported text for YouTube, you might want to check:\n"
                                "1. The start time of 00:00 must have a chapter.\n"
                                "2. There must be at least three timestamps in ascending order.\n"
                                "3. The minimum length for video chapters is 10 seconds."));
    messageWidget->setVisible(false);

    updateContentByModel();

    QPushButton *btn = buttonBox->addButton(i18n("Copy to Clipboard"), QDialogButtonBox::ActionRole);
    btn->setIcon(QIcon::fromTheme("edit-copy"));
    QPushButton *btn2 = buttonBox->addButton(i18n("Save"), QDialogButtonBox::ActionRole);
    btn2->setIcon(QIcon::fromTheme("document-save"));

    connect(categoryChooser, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { updateContentByModel(); });

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

    connect(btn2, &QAbstractButton::clicked, this, [this]() {
        QString filter = format_text->isChecked() ? QString("%1 (*.txt)").arg(i18n("Text File")) : QString("%1 (*.json)").arg(i18n("JSON File"));
        const QString startFolder = pCore->projectManager()->current()->projectDataFolder();
        QString filename = QFileDialog::getSaveFileName(this, i18nc("@title:window", "Export Guides Data"), startFolder, filter);
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            messageWidget->setText(i18n("Cannot write to file %1", QUrl::fromLocalFile(filename).fileName()));
            messageWidget->setMessageType(KMessageWidget::Warning);
            messageWidget->animatedShow();
            return;
        }
        file.write(generatedContent->toPlainText().toUtf8());
        file.close();
        messageWidget->setText(i18n("Guides saved to %1", QUrl::fromLocalFile(filename).fileName()));
        messageWidget->setMessageType(KMessageWidget::Positive);
        messageWidget->animatedShow();
    });

    connect(format_json, &QRadioButton::toggled, this, [this](bool jsonFormat) {
        textOptions->setEnabled(!jsonFormat);
        updateContentByModel();
    });

    connect(buttonReset, &QAbstractButton::clicked, [this, defaultFormat]() {
        formatEdit->setText(defaultFormat);
        updateContentByModel();
    });

    // fill info button menu
    QMap<QString, QString> infoMenu;
    infoMenu.insert(QStringLiteral("{{index}}"), i18n("Guide number"));
    infoMenu.insert(QStringLiteral("{{realtimecode}}"), i18n("Guide position in HH:MM:SS:FF"));
    infoMenu.insert(QStringLiteral("{{timecode}}"), i18n("Guide position in (HH:)MM.SS"));
    infoMenu.insert(QStringLiteral("{{nexttimecode}}"), i18n("Next guide position in (HH:)MM.SS"));
    infoMenu.insert(QStringLiteral("{{frame}}"), i18n("Guide position in frames"));
    infoMenu.insert(QStringLiteral("{{comment}}"), i18n("Guide comment"));
    QMapIterator<QString, QString> i(infoMenu);
    QAction *a;
    while (i.hasNext()) {
        i.next();
        a = new QAction(this);
        a->setText(QString("%1 - %2").arg(i.value(), i.key()));
        a->setData(i.key());
        infoButton->addAction(a);
    }
    connect(infoButton, &QToolButton::triggered, [this](QAction *a) {
        formatEdit->insert(a->data().toString());
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
    const int markerIndex = categoryChooser->currentCategory();
    if (format_json->isChecked()) {
        generatedContent->setPlainText(m_markerListModel->toJson({markerIndex}));
        return;
    }
    const QString format(formatEdit->text());
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
