/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "slideshowclip.h"
#include "bin/projectclip.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"

#include <KFileItem>
#include <KLocalizedString>
#include <KRecentDirs>

#include "kdenlive_debug.h"
#include <QDir>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QtMath>

SlideshowClip::SlideshowClip(const Timecode &tc, QString clipFolder, ProjectClip *clip, QWidget *parent)
    : QDialog(parent)
    , m_count(0)
    , m_timecode(tc)
    , m_thumbJob(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_view.setupUi(this);
    setWindowTitle(i18nc("@title:window", "Add Image Sequence"));
    if (clip) {
        m_view.clip_name->setText(clip->name());
        m_view.groupBox->setHidden(true);
    } else {
        m_view.clip_name->setText(i18n("Image Sequence"));
    }
    m_view.folder_url->setMode(KFile::Directory);
    m_view.folder_url->setUrl(QUrl::fromLocalFile(KRecentDirs::dir(QStringLiteral(":KdenliveSlideShowFolder"))));
    m_view.icon_list->setIconSize(QSize(50, 50));
    m_view.show_thumbs->setChecked(KdenliveSettings::showslideshowthumbs());

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_view.show_thumbs, &QCheckBox::checkStateChanged, this, &SlideshowClip::slotEnableThumbs);
    connect(m_view.slide_fade, &QCheckBox::checkStateChanged, this, &SlideshowClip::slotEnableLuma);
    connect(m_view.luma_fade, &QCheckBox::checkStateChanged, this, &SlideshowClip::slotEnableLumaFile);
#else
    connect(m_view.show_thumbs, &QCheckBox::stateChanged, this, &SlideshowClip::slotEnableThumbs);
    connect(m_view.slide_fade, &QCheckBox::stateChanged, this, &SlideshowClip::slotEnableLuma);
    connect(m_view.luma_fade, &QCheckBox::stateChanged, this, &SlideshowClip::slotEnableLumaFile);
#endif
    // WARNING: keep in sync with project/clipproperties.cpp
    m_view.image_type->addItem(QStringLiteral("JPG (*.jpg)"), QStringLiteral("jpg"));
    m_view.image_type->addItem(QStringLiteral("JPEG (*.jpeg)"), QStringLiteral("jpeg"));
    m_view.image_type->addItem(QStringLiteral("PNG (*.png)"), QStringLiteral("png"));
    m_view.image_type->addItem(QStringLiteral("SVG (*.svg)"), QStringLiteral("svg"));
    m_view.image_type->addItem(QStringLiteral("BMP (*.bmp)"), QStringLiteral("bmp"));
    m_view.image_type->addItem(QStringLiteral("GIF (*.gif)"), QStringLiteral("gif"));
    m_view.image_type->addItem(QStringLiteral("TGA (*.tga)"), QStringLiteral("tga"));
    m_view.image_type->addItem(QStringLiteral("TIF (*.tif)"), QStringLiteral("tif"));
    m_view.image_type->addItem(QStringLiteral("TIFF (*.tiff)"), QStringLiteral("tiff"));
    m_view.image_type->addItem(QStringLiteral("Open EXR (*.exr)"), QStringLiteral("exr"));
    m_view.image_type->addItem(i18n("Preview from CR2 (*.cr2)"), QStringLiteral("cr2"));
    m_view.image_type->addItem(i18n("Preview from ARW (*.arw)"), QStringLiteral("arw"));
    m_view.animation->addItem(i18n("None"), QString());

    KConfig conf(QStringLiteral("slideanimations.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "slides");
    QMap<QString, QString> slideTranslations;
    slideTranslations.insert(QStringLiteral("Pan"), i18nc("Image Pan", "Pan"));
    slideTranslations.insert(QStringLiteral("Pan and zoom"), i18n("Pan and zoom"));
    slideTranslations.insert(QStringLiteral("Zoom"), i18n("Zoom"));
    const QStringList animValues = group.keyList();
    for (const auto &val : animValues) {
        if (slideTranslations.contains(val)) {
            m_view.animation->addItem(slideTranslations.value(val), val);
        } else {
            m_view.animation->addItem(val, val);
        }
    }

    m_view.clip_duration->setInputMask(m_timecode.mask());
    m_view.luma_duration->setInputMask(m_timecode.mask());
    m_view.luma_duration->setText(m_timecode.getTimecodeFromFrames(int(ceil(m_timecode.fps()))));

    if (clipFolder.isEmpty()) {
        clipFolder = QDir::homePath();
    }
    m_view.folder_url->setUrl(QUrl::fromLocalFile(QFileInfo(clipFolder).dir().absolutePath()));

    m_view.clip_duration_format->addItem(i18n("hh:mm:ss:ff"));
    m_view.clip_duration_format->addItem(i18n("Frames"));
    connect(m_view.clip_duration_format, SIGNAL(activated(int)), this, SLOT(slotUpdateDurationFormat(int)));
    m_view.clip_duration_frames->setHidden(true);
    m_view.luma_duration_frames->setHidden(true);
    if (clip) {
        QString url = clip->url();
        if (QFileInfo(url).fileName().startsWith(QLatin1String(".all."))) {
            // the image sequence is defined by MIME type
            m_view.method_mime->setChecked(true);
            m_view.folder_url->setText(QFileInfo(url).absolutePath());
            QString filter = QFileInfo(url).fileName();
            QString ext = filter.section(QLatin1Char('.'), -1);
            int ix = m_view.image_type->findData(ext);
            if (ix > -1) {
                m_view.image_type->setCurrentIndex(ix);
            }
        } else {
            // the image sequence is defined by pattern
            m_view.method_pattern->setChecked(true);
            m_view.pattern_url->setText(url);
        }
    } else {
        // Using clipfolder file path
        QFileInfo file(clipFolder);
        QString extension = KdenliveSettings::slideshowmimeextension();
        if (file.isFile()) {
            extension = file.suffix();
            m_view.folder_url->setText(file.dir().absolutePath());
        }
        if (!extension.isEmpty()) {
            int ix = m_view.image_type->findData(extension);
            if (ix > -1) {
                m_view.image_type->setCurrentIndex(ix);
            }
        }
        m_view.pattern_url->setUrl(QUrl::fromLocalFile(clipFolder));
        m_view.method_mime->setChecked(KdenliveSettings::slideshowbymime());
        slotMethodChanged(m_view.method_mime->isChecked());
        parseFolder();
    }
    connect(m_view.method_mime, &QAbstractButton::toggled, this, &SlideshowClip::slotMethodChanged);
    connect(m_view.image_type, SIGNAL(currentIndexChanged(int)), this, SLOT(parseFolder()));
    connect(m_view.folder_url, &KUrlRequester::textChanged, this, &SlideshowClip::parseFolder);
    connect(m_view.pattern_url, &KUrlRequester::textChanged, this, &SlideshowClip::parseFolder);

    // Fill luma list
    m_view.luma_file->setIconSize(QSize(50, 30));
    const QStringList values = pCore->getLumasForProfile();
    QStringList names;
    for (const QString &value : std::as_const(values)) {
        names.append(QUrl(value).fileName());
    }
    for (int i = 0; i < values.count(); i++) {
        const QString &entry = values.at(i);
        // Create thumbnails
        if (!entry.isEmpty() && (entry.endsWith(QLatin1String(".png")) || entry.endsWith(QLatin1String(".pgm")))) {
            if (MainWindow::m_lumacache.contains(entry)) {
                m_view.luma_file->addItem(QPixmap::fromImage(MainWindow::m_lumacache.value(entry)), names.at(i), entry);
            } else {
                QImage pix(entry);
                if (!pix.isNull()) {
                    MainWindow::m_lumacache.insert(entry, pix.scaled(50, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    m_view.luma_file->addItem(QPixmap::fromImage(MainWindow::m_lumacache.value(entry)), names.at(i), entry);
                }
            }
        }
    }

    if (clip) {
        m_view.slide_loop->setChecked(clip->getProducerIntProperty(QStringLiteral("loop")) != 0);
        m_view.slide_crop->setChecked(clip->getProducerIntProperty(QStringLiteral("crop")) != 0);
        m_view.slide_fade->setChecked(clip->getProducerIntProperty(QStringLiteral("fade")) != 0);
        m_view.luma_softness->setValue(clip->getProducerIntProperty(QStringLiteral("softness")));
        QString anim = clip->getProducerProperty(QStringLiteral("animation"));
        if (!anim.isEmpty()) {
            int ix = m_view.animation->findData(anim);
            m_view.animation->setCurrentIndex(qMax(0, ix));
        } else {
            m_view.animation->setCurrentIndex(0);
        }
        m_view.low_pass->setChecked(clip->getProducerProperty(QStringLiteral("low-pass")) == QLatin1String("1"));
        int ttl = clip->getProducerIntProperty(QStringLiteral("ttl"));
        m_view.clip_duration->setText(tc.getTimecodeFromFrames(ttl));
        m_view.clip_duration_frames->setValue(ttl);
        m_view.luma_duration->setText(tc.getTimecodeFromFrames(clip->getProducerIntProperty(QStringLiteral("luma_duration"))));
        QString lumaFile = clip->getProducerProperty(QStringLiteral("luma_file"));
        if (!lumaFile.isEmpty()) {
            m_view.luma_fade->setChecked(true);
            m_view.luma_file->setCurrentIndex(m_view.luma_file->findData(lumaFile));
        } else {
            m_view.luma_file->setEnabled(false);
        }
        slotEnableLuma(m_view.slide_fade->checkState());
        slotEnableLumaFile(m_view.luma_fade->checkState());
        parseFolder();
    }
    m_view.low_pass->setEnabled(!m_view.animation->currentData().toString().isEmpty());
    connect(m_view.animation, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [&]() { m_view.low_pass->setEnabled(!m_view.animation->currentData().toString().isEmpty()); });
    // adjustSize();
}

SlideshowClip::~SlideshowClip()
{
    delete m_thumbJob;
}

void SlideshowClip::slotEnableLuma(int state)
{
    bool enable = false;
    if (state == Qt::Checked) {
        enable = true;
    }
    m_view.luma_duration->setEnabled(enable);
    m_view.luma_duration_frames->setEnabled(enable);
    m_view.luma_fade->setEnabled(enable);
    if (enable) {
        m_view.luma_file->setEnabled(m_view.luma_fade->isChecked());
    } else {
        m_view.luma_file->setEnabled(false);
    }
    m_view.label_softness->setEnabled(m_view.luma_fade->isChecked() && enable);
    m_view.luma_softness->setEnabled(m_view.label_softness->isEnabled());
}

void SlideshowClip::slotEnableThumbs(int state)
{
    if (state == Qt::Checked) {
        KdenliveSettings::setShowslideshowthumbs(true);
        slotGenerateThumbs();
    } else {
        KdenliveSettings::setShowslideshowthumbs(false);
        if (m_thumbJob) {
            disconnect(m_thumbJob, &KIO::PreviewJob::gotPreview, this, &SlideshowClip::slotSetPixmap);
            m_thumbJob->kill();
            m_thumbJob->deleteLater();
            m_thumbJob = nullptr;
        }
    }
}

void SlideshowClip::slotEnableLumaFile(int state)
{
    bool enable = false;
    if (state == Qt::Checked) {
        enable = true;
    }
    m_view.luma_file->setEnabled(enable);
    m_view.luma_softness->setEnabled(enable);
    m_view.label_softness->setEnabled(enable);
}

void SlideshowClip::parseFolder()
{
    m_view.icon_list->clear();
    bool isMime = m_view.method_mime->isChecked();
    QString path = isMime ? m_view.folder_url->url().toLocalFile() : m_view.pattern_url->url().adjusted(QUrl::RemoveFilename).toLocalFile();
    QDir dir(path);
    if (path.isEmpty() || !dir.exists()) {
        m_count = 0;
        m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        m_view.label_info->setText(QString());
        return;
    }

    QIcon unknownicon(QStringLiteral("unknown"));
    QStringList result;
    QStringList filters;
    QString filter;
    if (isMime) {
        // TODO: improve jpeg image detection with extension like jpeg, requires change in MLT image producers
        filter = m_view.image_type->currentData().toString();
        filters << QStringLiteral("*.") + filter;
        dir.setNameFilters(filters);
        result = dir.entryList(QDir::Files);
    } else {
        int offset = 0;
        QString path_pattern = m_view.pattern_url->text();
        QFileInfo filterInfo(path_pattern);
        QDir abs_dir = filterInfo.absoluteDir();
        result = abs_dir.entryList(QDir::Files);
        // find pattern
        if (path_pattern.contains(QLatin1Char('?'))) {
            // New MLT syntax
            if (path_pattern.section(QLatin1Char('?'), 1).contains(QLatin1Char(':'))) {
                // Old deprecated format
                offset = path_pattern.section(QLatin1Char(':'), -1).toInt();
            } else {
                offset = path_pattern.section(QLatin1Char('='), -1).toInt();
            }
            path_pattern = path_pattern.section(QLatin1Char('?'), 0, 0);
        }

        filter = filterInfo.completeBaseName();
        const QString ext = filterInfo.suffix();
        if (filter.contains(QLatin1Char('%'))) {
            filter = filter.section(QLatin1Char('%'), 0, -2);
        } else {
            while (!filter.isEmpty() && filter.at(filter.size() - 1).isDigit()) {
                filter.remove(filter.size() - 1, 1);
            }
        }
        QString regexp = QLatin1Char('^') + filter + QStringLiteral("\\d+\\.") + ext + QLatin1Char('$');
        const QRegularExpression rx(regexp);
        QStringList entries;
        for (const QString &p : std::as_const(result)) {
            if (rx.match(p).hasMatch()) {
                if (offset > 0) {
                    // make sure our image is in the range we want (> begin)
                    int ix = p.section(filter, 1).section('.', 0, 0).toInt();
                    if (ix < offset) {
                        continue;
                    }
                }
                entries << p;
            }
        }
        result = entries;
    }
    for (const QString &p : std::as_const(result)) {
        auto *item = new QListWidgetItem(unknownicon, p);
        item->setData(Qt::UserRole, dir.filePath(p));
        m_view.icon_list->addItem(item);
    }
    m_count = m_view.icon_list->count();
    m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_count > 0);
    if (m_count == 0) {
        m_view.label_info->setText(i18n("No image found"));
    } else {
        m_view.label_info->setText(i18np("1 image found", "%1 images found", m_count));
    }
    if (m_view.show_thumbs->isChecked()) {
        slotGenerateThumbs();
    }
    m_view.icon_list->setCurrentRow(0);
}

void SlideshowClip::slotGenerateThumbs()
{
    delete m_thumbJob;
    KFileItemList fileList;
    for (int i = 0; i < m_view.icon_list->count(); ++i) {
        QListWidgetItem *item = m_view.icon_list->item(i);
        if (item) {
            QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty()) {
                KFileItem f(QUrl::fromLocalFile(path));
                f.setDelayedMimeTypes(true);
                fileList.append(f);
            }
        }
    }
    m_thumbJob = new KIO::PreviewJob(fileList, QSize(50, 50));
    m_thumbJob->setScaleType(KIO::PreviewJob::Scaled);
    m_thumbJob->setAutoDelete(false);
    connect(m_thumbJob, &KIO::PreviewJob::gotPreview, this, &SlideshowClip::slotSetPixmap);
    m_thumbJob->start();
}

void SlideshowClip::slotSetPixmap(const KFileItem &fileItem, const QPixmap &pix)
{
    for (int i = 0; i < m_view.icon_list->count(); ++i) {
        QListWidgetItem *item = m_view.icon_list->item(i);
        if (item) {
            QString path = item->data(Qt::UserRole).toString();
            if (path == fileItem.url().toLocalFile()) {
                item->setIcon(QIcon(pix));
                item->setData(Qt::UserRole, QString());
                break;
            }
        }
    }
}

QString SlideshowClip::selectedPath()
{
    QStringList list;
    QUrl url;
    if (m_view.method_mime->isChecked()) {
        url = m_view.folder_url->url();
    } else {
        url = m_view.pattern_url->url();
    }
    QString path = selectedPath(url, m_view.method_mime->isChecked(), QStringLiteral(".all.") + m_view.image_type->currentData().toString(), &list);
    m_count = list.count();
    return path;
}

// static
int SlideshowClip::getFrameNumberFromPath(const QUrl &path)
{
    QString filter = path.fileName();
    filter = filter.section(QLatin1Char('.'), 0, -2);
    int ix = filter.size() - 1;
    while (ix >= 0 && filter.at(ix).isDigit()) {
        ix--;
    }
    return filter.remove(0, ix + 1).toInt();
}

// static
QString SlideshowClip::selectedPath(const QUrl &url, bool isMime, QString extension, QStringList *list)
{
    QString folder;
    if (isMime) {
        folder = url.toLocalFile();
        if (!folder.endsWith(QLatin1Char('/'))) {
            folder.append(QLatin1Char('/'));
        }
        // Check how many files we have
        QDir dir(folder);
        QStringList filters;
        filters << QStringLiteral("*.") + extension.section(QLatin1Char('.'), -1);
        dir.setNameFilters(filters);
        *list = dir.entryList(QDir::Files);
    } else {
        folder = url.adjusted(QUrl::RemoveFilename).toLocalFile();
        QString filter = url.fileName();
        QString ext = QLatin1Char('.') + filter.section(QLatin1Char('.'), -1);
        filter = filter.section(QLatin1Char('.'), 0, -2);
        int fullSize = filter.size();
        QString firstFrameData = filter;

        while (filter.size() > 0 && filter.at(filter.size() - 1).isDigit()) {
            filter.chop(1);
        }

        // Find number of digits in sequence
        int precision = fullSize - filter.size();
        int firstFrame = QStringView(firstFrameData).right(precision).toInt();
        // Check how many files we have
        QDir dir(folder);
        QString path;
        int gap = 0;
        for (int i = firstFrame; gap < 100; ++i) {
            path = filter + QString::number(i).rightJustified(precision, QLatin1Char('0'), false) + ext;
            if (dir.exists(path)) {
                (*list).append(folder + path);
                gap = 0;
            } else {
                gap++;
            }
        }
        extension = filter + QStringLiteral("%0") + QString::number(precision) + QLatin1Char('d') + ext;
        if (firstFrame > 0) {
            extension.append(QStringLiteral("?begin=%1").arg(firstFrame));
        }
    }
    return folder + extension;
}

QString SlideshowClip::clipName() const
{
    return m_view.clip_name->text();
}

QString SlideshowClip::clipDuration() const
{
    if (m_view.clip_duration_format->currentIndex() == 1) {
        // we are in frames mode
        return m_timecode.getTimecodeFromFrames(m_view.clip_duration_frames->value());
    }
    return m_view.clip_duration->text();
}

int SlideshowClip::imageCount() const
{
    return m_count;
}

int SlideshowClip::softness() const
{
    return m_view.luma_softness->value();
}

bool SlideshowClip::loop() const
{
    return m_view.slide_loop->isChecked();
}

bool SlideshowClip::crop() const
{
    return m_view.slide_crop->isChecked();
}

bool SlideshowClip::fade() const
{
    return m_view.slide_fade->isChecked();
}

QString SlideshowClip::lumaDuration() const
{
    if (m_view.clip_duration_format->currentIndex() == 1) {
        // we are in frames mode
        return m_timecode.getTimecodeFromFrames(m_view.luma_duration_frames->value());
    }
    return m_view.luma_duration->text();
}

QString SlideshowClip::lumaFile() const
{
    if (!m_view.luma_fade->isChecked() || !m_view.luma_file->isEnabled()) {
        return QString();
    }
    return m_view.luma_file->itemData(m_view.luma_file->currentIndex()).toString();
}

QString SlideshowClip::animation() const
{
    return m_view.animation->currentData().toString();
}

void SlideshowClip::slotUpdateDurationFormat(int ix)
{
    bool framesFormat = ix == 1;
    if (framesFormat) {
        // switching to frames count, update widget
        m_view.clip_duration_frames->setValue(m_timecode.getFrameCount(m_view.clip_duration->text()));
        m_view.luma_duration_frames->setValue(m_timecode.getFrameCount(m_view.luma_duration->text()));
    } else {
        // switching to timecode format
        m_view.clip_duration->setText(m_timecode.getTimecodeFromFrames(m_view.clip_duration_frames->value()));
        m_view.luma_duration->setText(m_timecode.getTimecodeFromFrames(m_view.luma_duration_frames->value()));
    }
    m_view.clip_duration_frames->setHidden(!framesFormat);
    m_view.clip_duration->setHidden(framesFormat);
    m_view.luma_duration_frames->setHidden(!framesFormat);
    m_view.luma_duration->setHidden(framesFormat);
}

void SlideshowClip::slotMethodChanged(bool active)
{
    if (active) {
        // User wants MIME type image sequence
        m_view.clip_duration->setText(m_timecode.reformatSeparators(KdenliveSettings::image_duration()));
        m_view.stackedWidget->setCurrentIndex(0);
        KdenliveSettings::setSlideshowbymime(true);
    } else {
        // User wants pattern image sequence
        m_view.clip_duration->setText(m_timecode.reformatSeparators(KdenliveSettings::sequence_duration()));
        m_view.stackedWidget->setCurrentIndex(1);
        KdenliveSettings::setSlideshowbymime(false);
    }
    parseFolder();
}

int SlideshowClip::lowPass() const
{
    return m_view.low_pass->isEnabled() && m_view.low_pass->isChecked() ? 1 : 0;
}

// static
QString SlideshowClip::animationToGeometry(const QString &animation, int &ttl)
{
    // load animation profiles
    KConfig conf(QStringLiteral("slideanimations.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "slides");
    QString geometry;
    if (group.hasKey(animation)) {
        geometry = group.readEntry(animation);
    }
    int frames = geometry.count(QLatin1String("%d="));
    int frameNumber = ttl - 1;
    QString str = QStringLiteral("%d");
    for (int i = 0; i < frames; i++) {
        geometry.replace(geometry.indexOf(str), 2, QString::number(frameNumber));
        frameNumber = qFloor((i + 3) / 2) * ttl;
        frameNumber -= (i % 2);
    }
    return geometry;
}

const QString SlideshowClip::extension() const
{
    return m_view.image_type->currentData().toString();
}
