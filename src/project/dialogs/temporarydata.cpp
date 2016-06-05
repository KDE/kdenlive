/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "temporarydata.h"
#include "doc/kdenlivedoc.h"
#include "utils/KoIconUtils.h"

#include <KLocalizedString>
#include <KIO/DirectorySizeJob>
#include <KMessageBox>

#include <QVBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QLabel>
#include <QStandardPaths>
#include <QToolButton>

static QList <QColor> chartColors;

ChartWidget::ChartWidget(QWidget * parent) :
    QWidget(parent)
{
    QFontMetrics ft(font());
    int minHeight = ft.height() * 6;
    setMinimumSize(minHeight, minHeight);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_segments = QList <int>();
}

void ChartWidget::setSegments(QList <int> segments)
{
    m_segments = segments;
    update();
}

void ChartWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing);
    const QRectF clipRect = event->rect();
    painter.setClipRect(clipRect);
    int pieWidth = qMin(width(), height()) - 10;
    const QRectF pieRect(5, 5, pieWidth, pieWidth);
    int ix = 0;
    int previous = 0;
    foreach (int val, m_segments) {
        if (val == 0) continue;
        painter.setBrush(chartColors.at(ix));
        painter.drawPie(pieRect, previous, val * 16);
        previous = val * 16;
        ix ++;
    }
}

TemporaryData::TemporaryData(KdenliveDoc *doc, bool currentProjectOnly, QWidget * parent) :
    QDialog(parent)
    , m_doc(doc)
{
    chartColors << QColor(Qt::darkRed) << QColor(Qt::darkBlue)  << QColor(Qt::darkGreen) << QColor(Qt::darkMagenta);
    mCurrentSizes << 0 << 0 << 0 << 0;
    QVBoxLayout *lay = new QVBoxLayout;

    m_currentPage = new QWidget(this);
    m_currentPage->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_grid = new QGridLayout;
    m_currentPie = new ChartWidget(this);
    m_grid->addWidget(m_currentPie, 0, 0, 4, 1);
    QPalette pal(palette());
    QFontMetrics ft(font());
    int minHeight = ft.height() / 2;

    // Timeline preview data
    QLabel *color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(0));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    m_grid->addWidget(color, 0, 1, Qt::AlignCenter);
    QLabel *preview = new QLabel(i18n("Timeline Preview"), this);
    m_grid->addWidget(preview, 0, 2);
    m_previewSize = new QLabel(this);
    m_grid->addWidget(m_previewSize, 0, 3);
    QToolButton *del = new QToolButton(this);
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deletePreview);
    del->setEnabled(false);
    m_grid->addWidget(del, 0, 4);

    // Proxy clips
    color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(1));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    m_grid->addWidget(color, 1, 1, Qt::AlignCenter);
    preview = new QLabel(i18n("Proxy Clips"), this);
    m_grid->addWidget(preview, 1, 2);
    m_proxySize = new QLabel(this);
    m_grid->addWidget(m_proxySize, 1, 3);
    del = new QToolButton(this);
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteProxy);
    del->setEnabled(false);
    m_grid->addWidget(del, 1, 4);

    // Audio Thumbs
    color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(2));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    m_grid->addWidget(color, 2, 1, Qt::AlignCenter);
    preview = new QLabel(i18n("Audio Thumbnails"), this);
    m_grid->addWidget(preview, 2, 2);
    m_audioSize = new QLabel(this);
    m_grid->addWidget(m_audioSize, 2, 3);
    del = new QToolButton(this);
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteAudio);
    del->setEnabled(false);
    m_grid->addWidget(del, 2, 4);

    // Video Thumbs
    color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(3));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    m_grid->addWidget(color, 3, 1, Qt::AlignCenter);
    preview = new QLabel(i18n("Video Thumbnails"), this);
    m_grid->addWidget(preview, 3, 2);
    m_thumbSize = new QLabel(this);
    m_grid->addWidget(m_thumbSize, 3, 3);
    del = new QToolButton(this);
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteThumbs);
    del->setEnabled(false);
    m_grid->addWidget(del, 3, 4);

    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    m_grid->addWidget(sep, 4, 0, 1, 5);
    // Current total
    preview = new QLabel(i18n("Project total cache data"), this);
    m_grid->addWidget(preview, 5, 0, 1, 3);
    m_currentSize = new QLabel(this);
    m_grid->addWidget(m_currentSize, 5, 3);
    del = new QToolButton(this);
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteAll);
    del->setEnabled(false);
    m_grid->addWidget(del, 5, 4);

    m_currentPage->setLayout(m_grid);

    if (currentProjectOnly) {
        lay->addWidget(m_currentPage);
    } else {
        QTabWidget *tab = new QTabWidget(this);
        tab->addTab(m_currentPage, i18n("Current Project"));
        QWidget *allProjects = new QWidget(this);
        tab->addTab(allProjects, i18n("All Projects"));
        lay->addWidget(tab);
    }
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(buttonBox);
    setLayout(lay);
    updateDataInfo();
}

void TemporaryData::updateDataInfo()
{
    m_totalCurrent = 0;
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!dir.cd(m_doc->getDocumentProperty(QStringLiteral("documentid")))) {
        m_currentPage->setEnabled(false);
        return;
    }

    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(dir.absolutePath()));
    connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotPreviewSize);

    if (dir.exists("proxy")) {
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(dir.absoluteFilePath("proxy")));
        connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotProxySize);
    }
    if (dir.exists("audiothumbs")) {
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(dir.absoluteFilePath("audiothumbs")));
        connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotAudioSize);
    }
    if (dir.exists("videothumbs")) {
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(dir.absoluteFilePath("videothumbs")));
        connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotThumbSize);
    }
}

void TemporaryData::gotPreviewSize(KJob *job)
{
    KIO::DirectorySizeJob *sourceJob = (KIO::DirectorySizeJob *) job;
    qulonglong total = sourceJob->totalSize();
    QLayoutItem *button = m_grid->itemAtPosition(0, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    mCurrentSizes[0] = total;
    m_previewSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotProxySize(KJob *job)
{
    KIO::DirectorySizeJob *sourceJob = (KIO::DirectorySizeJob *) job;
    qulonglong total = sourceJob->totalSize();
    QLayoutItem *button = m_grid->itemAtPosition(1, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    mCurrentSizes[1] = total;
    m_proxySize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotAudioSize(KJob *job)
{
    KIO::DirectorySizeJob *sourceJob = (KIO::DirectorySizeJob *) job;
    qulonglong total = sourceJob->totalSize();
    QLayoutItem *button = m_grid->itemAtPosition(2, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    mCurrentSizes[2] = total;
    m_audioSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotThumbSize(KJob *job)
{
    KIO::DirectorySizeJob *sourceJob = (KIO::DirectorySizeJob *) job;
    qulonglong total = sourceJob->totalSize();
    QLayoutItem *button = m_grid->itemAtPosition(3, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    mCurrentSizes[3] = total;
    m_thumbSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::updateTotal()
{
    m_currentSize->setText(KIO::convertSize(m_totalCurrent));
    QLayoutItem *button = m_grid->itemAtPosition(5, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(m_totalCurrent > 0);
    }
    if (m_totalCurrent == 0) {
        return;
    }
    QList <int> segments;
    foreach(qulonglong size, mCurrentSizes) {
        segments << size * 360 / m_totalCurrent;
    }
    m_currentPie->setSegments(segments);
}

void TemporaryData::deletePreview()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!dir.cd(m_doc->getDocumentProperty(QStringLiteral("documentid")))) {
        m_currentPage->setEnabled(false);
        return;
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
            return;
    }
}

void TemporaryData::deleteProxy()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!dir.cd(m_doc->getDocumentProperty(QStringLiteral("documentid")))) {
        m_currentPage->setEnabled(false);
        return;
    }
    if (!dir.cd(QStringLiteral("proxy"))) {
        return;
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache proxy folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
            return;
    }
}

void TemporaryData::deleteAudio()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!dir.cd(m_doc->getDocumentProperty(QStringLiteral("documentid")))) {
        m_currentPage->setEnabled(false);
        return;
    }
    if (!dir.cd(QStringLiteral("audiothumbs"))) {
        return;
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache audio folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
            return;
    }
}

void TemporaryData::deleteThumbs()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!dir.cd(m_doc->getDocumentProperty(QStringLiteral("documentid")))) {
        m_currentPage->setEnabled(false);
        return;
    }
    if (!dir.cd(QStringLiteral("videothumbs"))) {
        return;
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache thumbnail folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
            return;
    }
}

void TemporaryData::deleteAll()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!dir.cd(m_doc->getDocumentProperty(QStringLiteral("documentid")))) {
        m_currentPage->setEnabled(false);
        return;
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in cache folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
            return;
    }
}
