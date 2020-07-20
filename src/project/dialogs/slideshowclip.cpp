/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "slideshowclip.h"
#include "bin/projectclip.h"
#include "kdenlivesettings.h"

#include <KFileItem>
#include <KRecentDirs>
#include <klocalizedstring.h>

#include "kdenlive_debug.h"
#include <QDir>
#include <QFontDatabase>
#include <QStandardPaths>

SlideshowClip::SlideshowClip(const Timecode &tc, QString clipFolder, ProjectClip *clip, QWidget *parent)
    : QDialog(parent)
    , m_count(0)
    , m_timecode(tc)
    , m_thumbJob(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_view.setupUi(this);
    setWindowTitle(i18n("Add Image Sequence"));
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

    connect(m_view.show_thumbs, &QCheckBox::stateChanged, this, &SlideshowClip::slotEnableThumbs);
    connect(m_view.slide_fade, &QCheckBox::stateChanged, this, &SlideshowClip::slotEnableLuma);
    connect(m_view.luma_fade, &QCheckBox::stateChanged, this, &SlideshowClip::slotEnableLumaFile);

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
    m_view.animation->addItem(i18n("None"), QString());
    m_view.animation->addItem(i18n("Pan"), QStringLiteral("Pan"));
    m_view.animation->addItem(i18n("Pan, low-pass"), QStringLiteral("Pan, low-pass"));
    m_view.animation->addItem(i18n("Pan and zoom"), QStringLiteral("Pan and zoom"));
    m_view.animation->addItem(i18n("Pan and zoom, low-pass"), QStringLiteral("Pan and zoom, low-pass"));
    m_view.animation->addItem(i18n("Zoom"), QStringLiteral("Zoom"));
    m_view.animation->addItem(i18n("Zoom, low-pass"), QStringLiteral("Zoom, low-pass"));

    m_view.clip_duration->setInputMask(m_timecode.mask());
    m_view.luma_duration->setInputMask(m_timecode.mask());
    m_view.luma_duration->setText(m_timecode.getTimecodeFromFrames(int(ceil(m_timecode.fps()))));

    if (clipFolder.isEmpty()) {
        clipFolder = QDir::homePath();
    }
    m_view.folder_url->setUrl(QUrl::fromLocalFile(clipFolder));

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
            for (int i = 0; i < m_view.image_type->count(); ++i) {
                if (m_view.image_type->itemData(i).toString() == ext) {
                    m_view.image_type->setCurrentIndex(i);
                    break;
                }
            }
        } else {
            // the image sequence is defined by pattern
            m_view.method_pattern->setChecked(true);
            m_view.pattern_url->setText(url);
        }
    } else {
        m_view.method_mime->setChecked(KdenliveSettings::slideshowbymime());
        slotMethodChanged(m_view.method_mime->isChecked());
    }
    connect(m_view.method_mime, &QAbstractButton::toggled, this, &SlideshowClip::slotMethodChanged);
    connect(m_view.image_type, SIGNAL(currentIndexChanged(int)), this, SLOT(parseFolder()));
    connect(m_view.folder_url, &KUrlRequester::textChanged, this, &SlideshowClip::parseFolder);
    connect(m_view.pattern_url, &KUrlRequester::textChanged, this, &SlideshowClip::parseFolder);

    // Check for Kdenlive installed luma files
    QStringList filters;
    filters << QStringLiteral("*.pgm") << QStringLiteral("*.png");

    const QStringList customLumas = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("lumas"), QStandardPaths::LocateDirectory);
    for (const QString &folder : customLumas) {
        QDir directory(folder);
        const QStringList filesnames = directory.entryList(filters, QDir::Files);
        for (const QString &fname : filesnames) {
            QString filePath = directory.absoluteFilePath(fname);
            m_view.luma_file->addItem(QIcon::fromTheme(filePath), fname, filePath);
        }
    }

    // Check for MLT lumas
    QString profilePath = KdenliveSettings::mltpath();
    QString folder = profilePath.section(QLatin1Char('/'), 0, -3);
    folder.append(QStringLiteral("/lumas/PAL")); // TODO: cleanup the PAL / NTSC mess in luma files
    QDir lumafolder(folder);
    QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
    for (const QString &fname : qAsConst(filesnames)) {
        QString filePath = lumafolder.absoluteFilePath(fname);
        m_view.luma_file->addItem(QIcon::fromTheme(filePath), fname, filePath);
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
        filter = m_view.image_type->itemData(m_view.image_type->currentIndex()).toString();
        filters << QStringLiteral("*.") + filter;
        dir.setNameFilters(filters);
        result = dir.entryList(QDir::Files);
    } else {
        int offset = 0;
        QString path_pattern = m_view.pattern_url->text();
        QDir abs_dir = QFileInfo(m_view.pattern_url->text()).absoluteDir();
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
        filter = QFileInfo(path_pattern).fileName();
        QString ext = filter.section(QLatin1Char('.'), -1);
        if (filter.contains(QLatin1Char('%'))) {
            filter = filter.section(QLatin1Char('%'), 0, -2);
        } else {
            filter = filter.section(QLatin1Char('.'), 0, -2);
            while (!filter.isEmpty() && filter.at(filter.count() - 1).isDigit()) {
                filter.remove(filter.count() - 1, 1);
            }
        }
        // qCDebug(KDENLIVE_LOG) << " / /" << path_pattern << " / " << ext << " / " << filter;
        QString regexp = QLatin1Char('^') + filter + QStringLiteral("\\d+\\.") + ext + QLatin1Char('$');
        QRegExp rx(regexp);
        QStringList entries;
        int ix;
        for (const QString &p : qAsConst(result)) {
            if (rx.exactMatch(p)) {
                if (offset > 0) {
                    // make sure our image is in the range we want (> begin)
                    ix = p.section(filter, 1).section(QLatin1Char('.'), 0, 0).toInt();
                    ix = p.section(filter, 1).section('.', 0, 0).toInt();
                    if (ix < offset) {
                        continue;
                    }
                }
                entries << p;
            }
        }
        result = entries;
    }
    for (const QString &p : qAsConst(result)) {
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
    QString path = selectedPath(url, m_view.method_mime->isChecked(),
                                QStringLiteral(".all.") + m_view.image_type->itemData(m_view.image_type->currentIndex()).toString(), &list);
    m_count = list.count();
    // qCDebug(KDENLIVE_LOG)<<"// SELECTED PATH: "<<path;
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
        int firstFrame = firstFrameData.rightRef(precision).toInt();

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
    // qCDebug(KDENLIVE_LOG) << "// FOUND " << (*list).count() << " items for " << url.toLocalFile();
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
    if (m_view.animation->itemData(m_view.animation->currentIndex()).isNull()) {
        return QString();
    }
    return m_view.animation->itemData(m_view.animation->currentIndex()).toString();
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

// static
QString SlideshowClip::animationToGeometry(const QString &animation, int &ttl)
{
    QString geometry;
    if (animation.startsWith(QLatin1String("Pan and zoom"))) {
        geometry = QString::asprintf(
            "0=0/0:100%%x100%%;%d=-14%%/-14%%:120%%x120%%;%d=-5%%/-5%%:110%%x110%%;%d=0/0:110%%x110%%;%d=0/-5%%:110%%x110%%;%d=-5%%/0:110%%x110%%", ttl - 1,
            ttl, ttl * 2 - 1, ttl * 2, ttl * 3 - 1);
        ttl *= 3;
    } else if (animation.startsWith(QLatin1String("Pan"))) {
        geometry = QString::asprintf("0=-5%%/-5%%:110%%x110%%;%d=0/0:110%%x110%%;%d=0/0:110%%x110%%;%d=0/-5%%:110%%x110%%;%d=0/-5%%:110%%x110%%;%d=-5%%/"
                                     "-5%%:110%%x110%%;%d=0/-5%%:110%%x110%%;%d=-5%%/0:110%%x110%%",
                                     ttl - 1, ttl, ttl * 2 - 1, ttl * 2, ttl * 3 - 1, ttl * 3, ttl * 4 - 1);
        ttl *= 4;
    } else if (animation.startsWith(QLatin1String("Zoom"))) {
        geometry = QString::asprintf("0=0/0:100%%x100%%;%d=-14%%/-14%%:120%%x120%%", ttl - 1);
    }
    return geometry;
}
