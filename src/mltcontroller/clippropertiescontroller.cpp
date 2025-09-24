/*
SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2025 Julius Künzel <julius.kuenzel@kde.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clippropertiescontroller.h"
#include "clipcontroller.h"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "utils/uiutils.h"
#include "widgets/choosecolorwidget.h"
#include "widgets/timecodedisplay.h"
#include <audio/audioStreamInfo.h>

#include <KDualAction>
#include <KLocalizedString>

#include <KFileMetaData/ExtractionResult>
#include <KFileMetaData/Extractor>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#include <KIO/Global>
#include <KIO/OpenFileManagerWindowJob>
#include <KMessageBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QMenu>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPainter>
#include <QProcess>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTextEdit>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <QtMath>

AnalysisTree::AnalysisTree(QWidget *parent)
    : QTreeWidget(parent)
{
    setRootIsDecorated(false);
    setColumnCount(2);
    setAlternatingRowColors(true);
    setHeaderHidden(true);
    setDragEnabled(true);
}

// virtual
QMimeData *AnalysisTree::mimeData(const QList<QTreeWidgetItem *> &list) const
{
    QString mimeData;
    for (QTreeWidgetItem *item : list) {
        if ((item->flags() & Qt::ItemIsDragEnabled) != 0) {
            mimeData.append(item->text(1));
        }
    }
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/geometry"), mimeData.toUtf8());
    return mime;
}

class ExtractionResult : public KFileMetaData::ExtractionResult
{
public:
    ExtractionResult(const QString &filename, const QString &mimetype, QMap<QString, QString> *results)
        : KFileMetaData::ExtractionResult(filename, mimetype, KFileMetaData::ExtractionResult::ExtractMetaData)
        , m_results(results)
    {
    }

    void append(const QString & /*text*/) override {}

    void addType(KFileMetaData::Type::Type /*type*/) override {}

    void add(KFileMetaData::Property::Property property, const QVariant &value) override
    {
        bool decode = false;
        switch (property) {
        case KFileMetaData::Property::Manufacturer:
        case KFileMetaData::Property::Model:
        case KFileMetaData::Property::ImageDateTime:
        case KFileMetaData::Property::BitRate:
        case KFileMetaData::Property::TrackNumber:
        case KFileMetaData::Property::ReleaseYear:
        case KFileMetaData::Property::Composer:
        case KFileMetaData::Property::Genre:
        case KFileMetaData::Property::Artist:
        case KFileMetaData::Property::Album:
        case KFileMetaData::Property::Title:
        case KFileMetaData::Property::Comment:
        case KFileMetaData::Property::Copyright:
        case KFileMetaData::Property::PhotoFocalLength:
        case KFileMetaData::Property::PhotoExposureTime:
        case KFileMetaData::Property::PhotoFNumber:
        case KFileMetaData::Property::PhotoApertureValue:
        case KFileMetaData::Property::PhotoWhiteBalance:
        case KFileMetaData::Property::PhotoGpsLatitude:
        case KFileMetaData::Property::PhotoGpsLongitude:
            decode = true;
            break;
        default:
            break;
        }
        if (decode) {
            KFileMetaData::PropertyInfo info(property);
            QString stringValue;
            if (info.valueType() == QMetaType::Type::QDateTime) {
                QLocale locale;
                stringValue = locale.toDateTime(value.toString(), QLocale::ShortFormat).toString();
            } else if (info.valueType() == QMetaType::Type::Int) {
                int val = value.toInt();
                if (property == KFileMetaData::Property::BitRate) {
                    // Adjust unit for bitrate
                    stringValue = QString::number(val / 1000) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s");
                } else {
                    stringValue = QString::number(val);
                }
            } else if (info.valueType() == QMetaType::Type::Double) {
                stringValue = QString::number(value.toDouble());
            } else {
                stringValue = value.toString();
            }
            if (!stringValue.isEmpty()) {
                m_results->insert(info.displayName(), stringValue);
            }
        }
    }

private:
    QMap<QString, QString> *m_results;
    Q_DISABLE_COPY(ExtractionResult)
};

ClipPropertiesController::ClipPropertiesController(const QString &clipName, ClipController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_id(controller->binId())
    , m_type(controller->clipType())
    , m_properties(new Mlt::Properties(controller->properties()))
    , m_sourceProperties(new Mlt::Properties())
    , m_audioStream(nullptr)
    , m_textEdit(nullptr)
    , m_audioStreamsView(nullptr)
    , m_activeAudioStreams(-1)
{
    m_controller->mirrorOriginalProperties(m_sourceProperties);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    auto *lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    m_clipLabel = new ElidedFileLinkLabel(this);

    if (m_type == ClipType::Color || m_type == ClipType::Timeline || m_controller->clipUrl().isEmpty()) {
        m_clipLabel->clear();
        m_clipLabel->setText(clipName);
    } else {
        m_clipLabel->setText(m_controller->clipUrl());
        m_clipLabel->setLink(m_controller->clipUrl());
    }
    lay->addWidget(m_clipLabel);
    m_warningMessage = new KMessageWidget(this);
    lay->addWidget(m_warningMessage);
    m_warningMessage->setCloseButtonVisible(false);
    m_warningMessage->setWordWrap(true);
    m_warningMessage->hide();
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(false);
    m_tabWidget->setTabPosition(QTabWidget::East);
    m_tabWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    lay->addWidget(m_tabWidget);
    setLayout(lay);

    constructFileInfoPage();
    auto forcePage = constructPropertiesPage();
    auto forceAudioPage = constructAudioPropertiesPage();
    constructMetadataPage();
    constructAnalysisPage();

    const QSize iconSize = m_tabWidget->tabBar()->iconSize();
    m_tabWidget->addTab(m_propertiesPage, QString());
    m_tabWidget->addTab(forcePage, QString());
    m_tabWidget->addTab(forceAudioPage, QString());
    m_tabWidget->addTab(m_metaPage, QString());
    m_tabWidget->addTab(m_analysisPage, QString());
    m_tabWidget->setTabIcon(0, UiUtils::rotatedIcon(QStringLiteral("edit-find"), iconSize));
    m_tabWidget->setTabToolTip(0, i18n("File info"));
    m_tabWidget->setWhatsThis(xi18nc("@info:whatsthis", "Displays detailed information about the file."));
    m_tabWidget->setTabIcon(1, UiUtils::rotatedIcon(QStringLiteral("document-edit"), iconSize));
    m_tabWidget->setTabToolTip(1, i18n("Properties"));
    m_tabWidget->setWhatsThis(xi18nc("@info:whatsthis", "Displays detailed information about the video data/codec."));
    m_tabWidget->setTabIcon(2, UiUtils::rotatedIcon(QStringLiteral("audio-volume-high"), iconSize));
    m_tabWidget->setTabToolTip(2, i18n("Audio Properties"));
    m_tabWidget->setWhatsThis(xi18nc("@info:whatsthis", "Displays detailed information about the audio streams/data/codec."));
    m_tabWidget->setTabIcon(3, UiUtils::rotatedIcon(QStringLiteral("view-grid"), iconSize));
    m_tabWidget->setTabToolTip(3, i18n("Metadata"));
    m_tabWidget->setTabIcon(4, UiUtils::rotatedIcon(QStringLiteral("visibility"), iconSize));
    m_tabWidget->setTabToolTip(4, i18n("Analysis"));
    m_tabWidget->setWhatsThis(xi18nc("@info:whatsthis", "Displays analysis data."));
    m_tabWidget->setCurrentIndex(KdenliveSettings::properties_panel_page());
    if (m_type == ClipType::Color) {
        m_tabWidget->setTabEnabled(0, false);
    }
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &ClipPropertiesController::updateTab);
}

ClipPropertiesController::~ClipPropertiesController()
{
    if (m_watcher.isRunning()) {
        m_closing = true;
        m_watcher.waitForFinished();
    }
}

void ClipPropertiesController::constructFileInfoPage()
{
    m_propertiesPage = new QWidget(this);
    auto *box = new QVBoxLayout;
    m_propertiesPage->setLayout(box);

    m_propertiesTree = new QTreeWidget(this);
    m_propertiesTree->setRootIsDecorated(false);
    m_propertiesTree->setColumnCount(2);
    m_propertiesTree->setAlternatingRowColors(true);
    m_propertiesTree->sortByColumn(0, Qt::AscendingOrder);
    m_propertiesTree->setHeaderHidden(true);
    box->addWidget(m_propertiesTree);

    fillProperties();
}

QHBoxLayout *ClipPropertiesController::comboboxProperty(const QString &label, const QString &propertyName, const QMap<QString, int> &options,
                                                        const QString &defaultValue)
{
    QString propertyValue = m_properties->get(propertyName.toUtf8().constData());

    m_originalProperties.insert(propertyName, propertyValue.isEmpty() ? QStringLiteral("-") : propertyValue);

    auto box = new QCheckBox(label, this);
    box->setObjectName(propertyName);

    // Construct the combo box
    auto combo = new QComboBox(this);
    combo->setObjectName(QStringLiteral("%1_value").arg(propertyName));

    QMapIterator<QString, int> i(options);
    while (i.hasNext()) {
        i.next();
        combo->addItem(i.key(), i.value());
    }

    // connect signals and initialize state
    connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this, propertyName, combo]() {
        QMap<QString, QString> properties;
        properties.insert(propertyName, QString::number(combo->currentData().toInt()));
        Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
        m_originalProperties = properties;
    });

    if (!propertyValue.isEmpty()) {
        combo->setCurrentIndex(combo->findData(propertyValue));
    } else if (!defaultValue.isEmpty()) {
        combo->setCurrentIndex(combo->findData(defaultValue));
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(box, &QCheckBox::checkStateChanged, this, &ClipPropertiesController::slotEnableForce);
#else
    connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
#endif
    connect(box, &QAbstractButton::toggled, combo, &QWidget::setEnabled);
    box->setChecked(!propertyValue.isEmpty());
    combo->setEnabled(!propertyValue.isEmpty());

    // put everything together
    auto hlay = new QHBoxLayout;
    hlay->addWidget(box);
    hlay->addWidget(combo);

    return hlay;
}

QHBoxLayout *ClipPropertiesController::doubleSpinboxProperty(const QString &label, const QString &propertyName, double maxValue, double defaultValue)
{
    QString propertyValue = m_properties->get(propertyName.toUtf8().constData());
    m_originalProperties.insert(propertyName, propertyValue.isEmpty() ? QStringLiteral("-") : propertyValue);

    QCheckBox *box = new QCheckBox(label, this);
    box->setObjectName(propertyName);

    // Construct the spin box
    auto *spin = new QDoubleSpinBox(this);
    spin->setObjectName(QStringLiteral("%1_value").arg(propertyName));
    spin->setMaximum(maxValue);

    // connect signals and initialize state
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this, propertyName](double value) {
        QMap<QString, QString> properties;
        properties.insert(propertyName, QString::number(value, 'f'));
        Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
        m_originalProperties = properties;
    });

    if (!propertyValue.isEmpty()) {
        spin->setValue(propertyValue.toDouble());
    } else if (defaultValue > 0) {
        spin->setValue(defaultValue);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(box, &QCheckBox::checkStateChanged, this, &ClipPropertiesController::slotEnableForce);
#else
    connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
#endif
    connect(box, &QAbstractButton::toggled, spin, &QWidget::setEnabled);
    box->setChecked(!propertyValue.isEmpty());
    spin->setEnabled(!propertyValue.isEmpty());

    // put everything together
    auto *hlay = new QHBoxLayout;
    hlay->addWidget(box);
    hlay->addWidget(spin);

    return hlay;
}

QHBoxLayout *ClipPropertiesController::proxyProperty(const QString &label, const QString &propertyName)
{
    QString proxy = m_properties->get(propertyName.toUtf8().constData());
    m_originalProperties.insert(propertyName, proxy);
    auto *hlay = new QHBoxLayout;
    auto *bg = new QGroupBox(this);
    bg->setCheckable(false);
    bg->setFlat(true);
    auto *groupLay = new QHBoxLayout;
    groupLay->setContentsMargins(0, 0, 0, 0);
    auto *pbox = new QCheckBox(label, this);
    pbox->setTristate(true);
    // Proxy codec label
    QLabel *lab = new QLabel(this);
    pbox->setObjectName(propertyName);
    bool hasProxy = m_controller->hasProxy();
    if (hasProxy) {
        bg->setToolTip(proxy);
        bool proxyReady = (QFileInfo(proxy).fileName() == QFileInfo(m_properties->get("resource")).fileName());
        if (proxyReady) {
            pbox->setCheckState(Qt::Checked);
            if (!m_properties->property_exists("video_index")) {
                // Probable an image proxy
                lab->setText(i18n("Image"));
            } else {
                lab->setText(m_properties->get(QStringLiteral("meta.media.%1.codec.name").arg(m_properties->get_int("video_index")).toUtf8().constData()));
            }
        } else {
            pbox->setCheckState(Qt::PartiallyChecked);
        }
    } else {
        pbox->setCheckState(Qt::Unchecked);
    }
    pbox->setEnabled(pCore->projectManager()->current()->useProxy());
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(pbox, &QCheckBox::checkStateChanged, this, [this, pbox](Qt::CheckState state) {
#else
    connect(pbox, &QCheckBox::stateChanged, this, [this, pbox](int state) {
#endif
        Q_EMIT requestProxy(state == Qt::PartiallyChecked);
        if (state == Qt::Checked) {
            QSignalBlocker bk(pbox);
            pbox->setCheckState(Qt::Unchecked);
        }
    });
    connect(this, &ClipPropertiesController::enableProxy, pbox, &QCheckBox::setEnabled);
    connect(this, &ClipPropertiesController::proxyModified, this, [this, pbox, bg, lab](const QString &pxy) {
        bool hasProxyClip = pxy.length() > 2;
        QSignalBlocker bk(pbox);
        pbox->setCheckState(hasProxyClip ? Qt::Checked : Qt::Unchecked);
        bg->setEnabled(pbox->isChecked());
        bg->setToolTip(pxy);
        lab->setText(hasProxyClip ? m_properties->get(QStringLiteral("meta.media.%1.codec.name").arg(m_properties->get_int("video_index")).toUtf8().constData())
                                  : QString());
    });
    hlay->addWidget(pbox);
    bg->setEnabled(pbox->checkState() == Qt::Checked);

    groupLay->addWidget(lab);

    // Delete button
    auto *tb = new QToolButton(this);
    tb->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    tb->setAutoRaise(true);
    connect(tb, &QToolButton::clicked, this, [this, proxy]() { Q_EMIT deleteProxy(); });
    tb->setToolTip(i18n("Delete proxy file"));
    groupLay->addWidget(tb);

    // Folder button
    tb = new QToolButton(this);
    auto *pMenu = new QMenu(this);
    tb->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    tb->setToolTip(i18n("Proxy options"));
    tb->setMenu(pMenu);
    tb->setAutoRaise(true);
    tb->setPopupMode(QToolButton::InstantPopup);

    QAction *ac = new QAction(QIcon::fromTheme(QStringLiteral("document-open")), i18n("Open folder…"), this);
    connect(ac, &QAction::triggered, this, [this, propertyName]() {
        QString pxy = m_properties->get(propertyName.toUtf8().constData());
        pCore->highlightFileInExplorer({QUrl::fromLocalFile(pxy)});
    });
    pMenu->addAction(ac);
    ac = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), i18n("Play proxy clip"), this);
    connect(ac, &QAction::triggered, this, [this, propertyName]() {
        QString pxy = m_properties->get(propertyName.toUtf8().constData());
        QDesktopServices::openUrl(QUrl::fromLocalFile(pxy));
    });
    pMenu->addAction(ac);
    ac = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy file location to clipboard"), this);
    connect(ac, &QAction::triggered, this, [this, propertyName]() {
        QString pxy = m_properties->get(propertyName.toUtf8().constData());
        QGuiApplication::clipboard()->setText(pxy);
    });
    pMenu->addAction(ac);

    groupLay->addWidget(tb);
    bg->setLayout(groupLay);
    hlay->addWidget(bg);

    return hlay;
}

QHBoxLayout *ClipPropertiesController::durationProperty(const QString &label, const QString &propertyName)
{
    QCheckBox *box = new QCheckBox(label, this);
    box->setObjectName(propertyName.toUtf8().constData());

    auto *timePos = new TimecodeDisplay(this);
    timePos->setObjectName(QStringLiteral("%1_value").arg(propertyName));

    m_originalProperties.insert(QStringLiteral("out"), m_properties->get("out"));
    int kdenlive_length = m_properties->time_to_frames(m_properties->get("kdenlive:duration"));
    if (kdenlive_length > 0) {
        m_originalProperties.insert(QStringLiteral("kdenlive:duration"), m_properties->get("kdenlive:duration"));
    }
    m_originalProperties.insert(QStringLiteral("length"), m_properties->get("length"));
    timePos->setValue(kdenlive_length > 0 ? kdenlive_length : m_properties->get_int("length"));

    int original_length = m_properties->get_int("kdenlive:original_length");
    if (original_length > 0) {
        box->setChecked(true);
    } else {
        timePos->setEnabled(false);
    }

    connect(box, &QAbstractButton::toggled, timePos, &QWidget::setEnabled);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(box, &QCheckBox::checkStateChanged, this, &ClipPropertiesController::slotEnableForce);
#else
    connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
#endif
    connect(timePos, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipPropertiesController::slotDurationChanged);
    connect(this, SIGNAL(durationModified(int)), timePos, SLOT(setValue(int)));

    auto *hlay = new QHBoxLayout;
    hlay->addWidget(box);
    hlay->addWidget(timePos);

    return hlay;
}

QHBoxLayout *ClipPropertiesController::timecodeProperty(const QString &label, const QString &propertyName)
{
    QLabel *labelWidget = new QLabel(label, this);
    auto *timePos = new TimecodeDisplay(this);
    timePos->setObjectName(propertyName);

    int propertyValue = 0;
    if (propertyName.startsWith(QLatin1String("kdenlive:sequenceproperties."))) {
        const QUuid uuid(m_properties->get("kdenlive:uuid"));
        propertyValue = pCore->currentDoc()->getSequenceProperty(uuid, propertyName).toInt();
    } else {
        propertyValue = m_properties->get_int(propertyName.toUtf8().constData());
    }
    m_originalProperties.insert(propertyName, QString::number(propertyValue));
    timePos->setValue(propertyValue);
    connect(timePos, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipPropertiesController::slotTimecodeChanged);
    connect(this, SIGNAL(timecodeModified(int)), timePos, SLOT(setValue(int)));

    auto *hlay = new QHBoxLayout;
    hlay->addWidget(labelWidget);
    hlay->addWidget(timePos);

    return hlay;
}

QHBoxLayout *ClipPropertiesController::aspectRatioProperty(const QString &label)
{
    double ratio = 0.;
    if (m_properties->property_exists("force_aspect_ratio")) {
        ratio = m_properties->get_double("force_aspect_ratio");
        m_originalProperties.insert(QStringLiteral("force_aspect_den"), QString::number(ratio));
    } else {
        m_originalProperties.insert(QStringLiteral("force_aspect_den"), QString());
    }
    QCheckBox *box = new QCheckBox(label, this);
    box->setObjectName(QStringLiteral("force_ar"));

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(box, &QCheckBox::checkStateChanged, this, &ClipPropertiesController::slotEnableForce);
#else
    connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
#endif

    auto *numSpin = new QSpinBox(this);
    numSpin->setMaximum(8000);
    numSpin->setObjectName(QStringLiteral("force_aspect_num_value"));

    auto *denSpin = new QSpinBox(this);
    denSpin->setMinimum(1);
    denSpin->setMaximum(8000);
    denSpin->setObjectName(QStringLiteral("force_aspect_den_value"));

    if (ratio == 0.) {
        // use current ratio
        int num = m_properties->get_int("meta.media.sample_aspect_num");
        int den = m_properties->get_int("meta.media.sample_aspect_den");
        if (den == 0) {
            num = 1;
            den = 1;
        }
        numSpin->setEnabled(false);
        denSpin->setEnabled(false);
        numSpin->setValue(num);
        denSpin->setValue(den);
    } else {
        box->setChecked(true);
        numSpin->setEnabled(true);
        denSpin->setEnabled(true);

        int ratio_num = qRound(ratio * 1000);
        int ratio_den = 1000;
        int num = ProfilesDialog::gcd(ratio_num, ratio_den);
        if (num > 0) {
            ratio_num /= num;
            ratio_den /= num;
        }
        numSpin->setValue(ratio_num);
        denSpin->setValue(ratio_den);
    }
    connect(denSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ClipPropertiesController::slotAspectValueChanged);
    connect(numSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ClipPropertiesController::slotAspectValueChanged);
    connect(box, &QAbstractButton::toggled, numSpin, &QWidget::setEnabled);
    connect(box, &QAbstractButton::toggled, denSpin, &QWidget::setEnabled);

    auto *hlay = new QHBoxLayout;
    hlay->addWidget(box);
    hlay->addStretch(10);
    hlay->addWidget(numSpin);
    hlay->addWidget(new QLabel(QStringLiteral(":")));
    hlay->addWidget(denSpin);

    return hlay;
}

QVBoxLayout *ClipPropertiesController::textProperty(const QString &label, const QString &propertyName)
{
    auto vlay = new QVBoxLayout;
    QString currentText = m_properties->get(propertyName.toUtf8().constData());
    m_originalProperties.insert(propertyName, currentText);
    m_textEdit = new QTextEdit(this);
    m_textEdit->setAcceptRichText(false);
    m_textEdit->setPlainText(currentText);
    m_textEdit->setPlaceholderText(label);
    vlay->addWidget(m_textEdit);
    QPushButton *button = new QPushButton(i18n("Apply"), this);
    vlay->addWidget(button);
    connect(button, &QPushButton::clicked, this, &ClipPropertiesController::slotTextChanged);

    return vlay;
}

QWidget *ClipPropertiesController::constructPropertiesPage()
{
    auto *forcePage = new QScrollArea(this);
    QWidget *forceProp = new QWidget(this);

    auto *fpBox = new QVBoxLayout;
    fpBox->setSpacing(0);
    forceProp->setLayout(fpBox);
    forcePage->setWidget(forceProp);
    forcePage->setWidgetResizable(true);

    if (m_type == ClipType::Text || m_type == ClipType::SlideShow || m_type == ClipType::TextTemplate) {
        QPushButton *editButton = new QPushButton(i18n("Edit Clip"), this);
        connect(editButton, &QAbstractButton::clicked, this, &ClipPropertiesController::editClip);
        fpBox->addWidget(editButton);
    }
    if (m_type == ClipType::Timeline) {
        // Edit duration widget
        auto hlay = timecodeProperty(i18n("Timecode Offset:"), QStringLiteral("kdenlive:sequenceproperties.timecodeOffset"));
        fpBox->addLayout(hlay);
    }
    if (m_type == ClipType::Color || m_type == ClipType::Image || m_type == ClipType::AV || m_type == ClipType::Video || m_type == ClipType::TextTemplate) {
        // Edit duration widget
        auto hlay = durationProperty(i18n("Duration:"), QStringLiteral("force_duration"));
        fpBox->addLayout(hlay);

        // Autorotate
        if (m_type == ClipType::Image) {
            int autorotate = m_properties->get_int("disable_exif");
            m_originalProperties.insert(QStringLiteral("disable_exif"), QString::number(autorotate));
            hlay = new QHBoxLayout;
            auto box = new QCheckBox(i18n("Disable autorotate"), this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            connect(box, &QCheckBox::checkStateChanged, this, &ClipPropertiesController::slotEnableForce);
#else
            connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
#endif
            box->setObjectName(QStringLiteral("disable_exif"));
            box->setChecked(autorotate == 1);
            hlay->addWidget(box);
            fpBox->addLayout(hlay);
        }
        // connect(this, static_cast<void(ClipPropertiesController::*)(int)>(&ClipPropertiesController::modified), timePos, &TimecodeDisplay::setValue);
    }
    if (m_type == ClipType::TextTemplate) {
        // Edit text widget
        auto vlay = textProperty(i18n("Enter template text here"), QStringLiteral("templatetext"));
        fpBox->addLayout(vlay);
    } else if (m_type == ClipType::Color) {
        // Edit color widget
        m_originalProperties.insert(QStringLiteral("resource"), m_properties->get("resource"));
        mlt_color color = m_properties->get_color("resource");

        QLabel *label = new QLabel(i18n("Color"), this);
        ChooseColorWidget *choosecolor = new ChooseColorWidget(this, QColor::fromRgb(color.r, color.g, color.b), false);

        auto *colorLay = new QHBoxLayout;
        colorLay->setContentsMargins(0, 0, 0, 0);
        colorLay->setSpacing(0);
        colorLay->addWidget(label);
        colorLay->addStretch();
        colorLay->addWidget(choosecolor);

        fpBox->addLayout(colorLay);
        connect(choosecolor, &ChooseColorWidget::modified, this, &ClipPropertiesController::slotColorModified);
        connect(this, &ClipPropertiesController::colorModified, choosecolor, &ChooseColorWidget::slotColorModified);
    }
    if (m_type == ClipType::AV || m_type == ClipType::Video || m_type == ClipType::Image) {
        // Aspect ratio
        auto hlay = aspectRatioProperty(i18n("Pixel aspect ratio:"));
        fpBox->addLayout(hlay);
    }

    if (m_controller->supportsProxy()) {
        // Proxy
        auto hlay = proxyProperty(i18n("Proxy clip"), QStringLiteral("kdenlive:proxy"));
        fpBox->addLayout(hlay);
    }

    if (m_type == ClipType::AV || m_type == ClipType::Video) {
        // Fps
        auto hlay = doubleSpinboxProperty(i18n("Frame rate:"), QStringLiteral("force_fps"), 1000, m_controller->originalFps());
        fpBox->addLayout(hlay);

        // Scanning
        QMap<QString, int> options = {{i18n("Interlaced"), 0}, {i18n("Progressive"), 1}};
        hlay = comboboxProperty(i18n("Scanning:"), QStringLiteral("force_progressive"), options);
        fpBox->addLayout(hlay);

        // Field order
        options = {{i18n("Bottom First"), 0}, {i18n("Top First"), 1}};
        hlay = comboboxProperty(i18n("Field order:"), QStringLiteral("force_tff"), options);
        fpBox->addLayout(hlay);

        // Rotate
        int vix = m_sourceProperties->get_int("video_index");
        const QString query = QStringLiteral("meta.media.%1.codec.rotate").arg(vix);
        int defaultRotate = m_sourceProperties->get_int(query.toUtf8().constData());
        options.clear();
        for (int i = 0; i < 4; i++) {
            int a = 90 * i;
            if (a == defaultRotate) {
                options.insert(i18n("%1 (default)", QStringLiteral("%1°").arg(a)), a);
            } else {
                options.insert(QStringLiteral("%1°").arg(a), a);
            }
        }
        hlay = comboboxProperty(i18n("Rotate:"), QStringLiteral("rotate"), options, QString::number(defaultRotate));
        fpBox->addLayout(hlay);

        // Video index
        if (!m_videoStreams.isEmpty()) {
            QString vix = m_properties->get("video_index");
            m_originalProperties.insert(QStringLiteral("video_index"), vix);
            hlay = new QHBoxLayout;

            KDualAction *ac = new KDualAction(i18n("Disable video"), i18n("Enable video"), this);
            ac->setInactiveIcon(QIcon::fromTheme(QStringLiteral("video")));
            ac->setActiveIcon(QIcon::fromTheme(QStringLiteral("video-off")));
            auto *tbv = new QToolButton(this);
            tbv->setToolButtonStyle(Qt::ToolButtonIconOnly);
            tbv->setDefaultAction(ac);
            tbv->setAutoRaise(true);
            hlay->addWidget(tbv);
            hlay->addWidget(new QLabel(i18n("Video stream")));
            auto *videoStream = new QComboBox(this);
            int ix = 1;
            for (int stream : std::as_const(m_videoStreams)) {
                videoStream->addItem(i18n("Video stream %1", ix), stream);
                ix++;
            }
            if (!vix.isEmpty() && vix.toInt() > -1) {
                videoStream->setCurrentIndex(videoStream->findData(QVariant(vix)));
            }
            ac->setActive(vix.toInt() == -1);
            videoStream->setEnabled(vix.toInt() > -1);
            videoStream->setVisible(m_videoStreams.size() > 1);
            connect(ac, &KDualAction::activeChanged, this, [this, videoStream](bool activated) {
                QMap<QString, QString> properties;
                int vindx = -1;
                if (activated) {
                    videoStream->setEnabled(false);
                } else {
                    videoStream->setEnabled(true);
                    vindx = videoStream->currentData().toInt();
                }
                properties.insert(QStringLiteral("video_index"), QString::number(vindx));
                properties.insert(QStringLiteral("set.test_image"), vindx > -1 ? QStringLiteral("0") : QStringLiteral("1"));
                Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
                m_originalProperties = properties;
            });
            QObject::connect(videoStream, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this, videoStream]() {
                QMap<QString, QString> properties;
                properties.insert(QStringLiteral("video_index"), QString::number(videoStream->currentData().toInt()));
                Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
                m_originalProperties = properties;
            });
            hlay->addWidget(videoStream);
            fpBox->addLayout(hlay);
        }

        // Colorspace
        options = {{ProfileRepository::getColorspaceDescription(170), 170}, {ProfileRepository::getColorspaceDescription(240), 240},
                   {ProfileRepository::getColorspaceDescription(601), 601}, {ProfileRepository::getColorspaceDescription(709), 709},
                   {ProfileRepository::getColorspaceDescription(470), 470}, {ProfileRepository::getColorspaceDescription(2020), 2020}};

        QString defaultValue;
        int colorspace = m_controller->getProducerIntProperty(QStringLiteral("meta.media.colorspace"));
        if (colorspace > 0) {
            defaultValue = QString::number(colorspace);
        }

        hlay = comboboxProperty(i18n("Color space:"), QStringLiteral("force_colorspace"), options, defaultValue);
        fpBox->addLayout(hlay);

        // Color range
        options = {{i18n("Broadcast limited (MPEG)"), 1}, {i18n("Full (JPEG)"), 2}};
        defaultValue = QString::number(m_controller->isFullRange() ? 2 : 1);
        hlay = comboboxProperty(i18n("Color range:"), QStringLiteral("color_range"), options, defaultValue);
        fpBox->addLayout(hlay);

        // Check for variable frame rate
        if (m_properties->get_int("meta.media.variable_frame_rate")) {
            m_warningMessage->setText(i18n("File uses a variable frame rate, not recommended"));
            QAction *ac = new QAction(i18n("Transcode"));
            QObject::connect(ac, &QAction::triggered, [id = m_id, resource = m_controller->clipUrl()]() {
                QMetaObject::invokeMethod(pCore->bin(), "requestTranscoding", Qt::QueuedConnection, Q_ARG(QString, resource), Q_ARG(QString, id), Q_ARG(int, 0),
                                          Q_ARG(bool, false));
            });
            m_warningMessage->setMessageType(KMessageWidget::Warning);
            m_warningMessage->addAction(ac);
            m_warningMessage->show();
        } else {
            m_warningMessage->hide();
        }
    }
    fpBox->addStretch(10);

    return forcePage;
}

QWidget *ClipPropertiesController::constructAudioPropertiesPage()
{
    auto *forceAudioPage = new QScrollArea(this);

    // Force Audio properties
    auto *audioVbox = new QVBoxLayout;
    audioVbox->setSpacing(0);

    // Audio index
    QMap<int, QString> audioStreamsInfo = m_controller->audioStreams();
    if (!audioStreamsInfo.isEmpty()) {
        QList<int> enabledStreams = m_controller->activeStreams().keys();
        QString vix = m_sourceProperties->get("audio_index");
        m_originalProperties.insert(QStringLiteral("audio_index"), vix);
        QStringList streamString;
        for (int streamIx : std::as_const(enabledStreams)) {
            streamString << QString::number(streamIx);
        }
        m_originalProperties.insert(QStringLiteral("kdenlive:active_streams"), streamString.join(QLatin1Char(';')));
        auto hlay = new QHBoxLayout;

        KDualAction *ac = new KDualAction(i18n("Disable audio"), i18n("Enable audio"), this);
        ac->setInactiveIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
        ac->setActiveIcon(QIcon::fromTheme(QStringLiteral("audio-off")));
        auto *tbv = new QToolButton(this);
        tbv->setToolButtonStyle(Qt::ToolButtonIconOnly);
        tbv->setDefaultAction(ac);
        tbv->setAutoRaise(true);
        hlay->addWidget(tbv);
        hlay->addWidget(new QLabel(i18n("Audio streams")));
        audioVbox->addLayout(hlay);
        m_audioStreamsView = new QListWidget(this);
        m_audioStreamsView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        audioVbox->addWidget(m_audioStreamsView);
        QMapIterator<int, QString> i(audioStreamsInfo);
        while (i.hasNext()) {
            i.next();
            auto *item = new QListWidgetItem(i.value(), m_audioStreamsView);
            // Store stream index
            item->setData(Qt::UserRole, i.key());
            // Store oringinal name
            item->setData(Qt::UserRole + 1, i.value());
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
            if (enabledStreams.contains(i.key())) {
                item->setCheckState(Qt::Checked);
            } else {
                item->setCheckState(Qt::Unchecked);
            }
            updateStreamIcon(m_audioStreamsView->row(item), i.key());
        }
        if (audioStreamsInfo.count() > 1) {
            QListWidgetItem *item = new QListWidgetItem(i18n("Merge all streams"), m_audioStreamsView);
            item->setData(Qt::UserRole, INT_MAX);
            item->setData(Qt::UserRole + 1, item->text());
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (enabledStreams.contains(INT_MAX)) {
                item->setCheckState(Qt::Checked);
            } else {
                item->setCheckState(Qt::Unchecked);
            }
        }
        connect(m_audioStreamsView, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row > -1) {
                m_audioEffectGroup->setEnabled(true);
                QListWidgetItem *item = m_audioStreamsView->item(row);
                m_activeAudioStreams = item->data(Qt::UserRole).toInt();
                QStringList effects = m_controller->getAudioStreamEffect(m_activeAudioStreams);
                QSignalBlocker bk(m_swapChannels);
                QSignalBlocker bk1(m_copyChannelGroup);
                QSignalBlocker bk2(m_normalize);
                m_swapChannels->setChecked(effects.contains(QLatin1String("channelswap")));
                m_copyChannel1->setChecked(effects.contains(QStringLiteral("channelcopy from=0 to=1")));
                m_copyChannel2->setChecked(effects.contains(QStringLiteral("channelcopy from=1 to=0")));
                m_normalize->setChecked(effects.contains(QStringLiteral("dynamic_loudness")));
                int gain = 0;
                for (const QString &st : std::as_const(effects)) {
                    if (st.startsWith(QLatin1String("volume "))) {
                        QSignalBlocker bk3(m_gain);
                        gain = st.section(QLatin1Char('='), 1).toInt();
                        break;
                    }
                }
                QSignalBlocker bk3(m_gain);
                m_gain->setValue(gain);
            } else {
                m_activeAudioStreams = -1;
                m_audioEffectGroup->setEnabled(false);
            }
        });
        connect(m_audioStreamsView, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
            if (!item) {
                return;
            }
            bool checked = item->checkState() == Qt::Checked;
            int streamId = item->data(Qt::UserRole).toInt();
            bool streamModified = false;
            QString currentStreams = m_originalProperties.value(QStringLiteral("kdenlive:active_streams"));
            QStringList activeStreams = currentStreams.split(QLatin1Char(';'));
            if (activeStreams.contains(QString::number(streamId))) {
                if (!checked) {
                    // Stream was unselected
                    activeStreams.removeAll(QString::number(streamId));
                    streamModified = true;
                }
            } else if (checked) {
                // Stream was selected
                if (streamId == INT_MAX) {
                    // merge all streams should not have any other stream selected
                    activeStreams.clear();
                } else {
                    activeStreams.removeAll(QString::number(INT_MAX));
                }
                activeStreams << QString::number(streamId);
                activeStreams.sort();
                streamModified = true;
            }
            if (streamModified) {
                if (activeStreams.isEmpty()) {
                    activeStreams << QStringLiteral("-1");
                }
                QMap<QString, QString> properties;
                properties.insert(QStringLiteral("kdenlive:active_streams"), activeStreams.join(QLatin1Char(';')));
                Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
                m_originalProperties = properties;
            } else if (item->text() != item->data(Qt::UserRole + 1).toString()) {
                // Rename event
                QString txt = item->text();
                int row = m_audioStreamsView->row(item) + 1;
                if (!txt.startsWith(QStringLiteral("%1|").arg(row))) {
                    txt.prepend(QStringLiteral("%1|").arg(row));
                }
                m_controller->renameAudioStream(streamId, txt);
                QSignalBlocker bk(m_audioStreamsView);
                item->setText(txt);
                item->setData(Qt::UserRole + 1, txt);
            }
        });
        ac->setActive(vix.toInt() == -1);
        connect(ac, &KDualAction::activeChanged, this, [this, audioStreamsInfo](bool activated) {
            QMap<QString, QString> properties;
            int vindx = -1;
            if (activated) {
                properties.insert(QStringLiteral("kdenlive:active_streams"), QStringLiteral("-1"));
            } else {
                properties.insert(QStringLiteral("kdenlive:active_streams"), QString());
                vindx = audioStreamsInfo.firstKey();
            }
            properties.insert(QStringLiteral("audio_index"), QString::number(vindx));
            // Find stream position in index
            QMap<int, QString>::const_iterator it = audioStreamsInfo.constFind(vindx);
            if (it != audioStreamsInfo.constEnd()) {
                properties.insert(QStringLiteral("astream"), QString::number(std::distance(audioStreamsInfo.begin(), it)));
            }
            properties.insert(QStringLiteral("set.test_audio"), vindx > -1 ? QStringLiteral("0") : QStringLiteral("1"));
            Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
            m_originalProperties = properties;
        });
        // Audio effects
        m_audioEffectGroup = new QGroupBox(this);
        m_audioEffectGroup->setEnabled(false);
        auto *vbox = new QVBoxLayout;
        // Normalize
        m_normalize = new QCheckBox(i18n("Normalize"), this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(m_normalize, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
#else
        connect(m_normalize, &QCheckBox::stateChanged, this, [this](int state) {
#endif
            if (m_activeAudioStreams == -1) {
                // No stream selected, abort
                return;
            }
            if (state == Qt::Checked) {
                // Add swap channels effect
                m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("dynamic_loudness"));
            } else {
                // Remove swap channels effect
                m_controller->requestRemoveStreamEffect(m_activeAudioStreams, QStringLiteral("dynamic_loudness"));
            }
            updateStreamIcon(m_audioStreamsView->currentRow(), m_activeAudioStreams);
        });
        vbox->addWidget(m_normalize);

        // Swap channels
        m_swapChannels = new QCheckBox(i18n("Swap channels"), this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(m_swapChannels, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
#else
        connect(m_swapChannels, &QCheckBox::stateChanged, this, [this](int state) {
#endif
            if (m_activeAudioStreams == -1) {
                // No stream selected, abort
                return;
            }
            if (state == Qt::Checked) {
                // Add swap channels effect
                m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("channelswap"));
            } else {
                // Remove swap channels effect
                m_controller->requestRemoveStreamEffect(m_activeAudioStreams, QStringLiteral("channelswap"));
            }
            updateStreamIcon(m_audioStreamsView->currentRow(), m_activeAudioStreams);
        });
        vbox->addWidget(m_swapChannels);
        // Copy channel
        auto *copyLay = new QHBoxLayout;
        copyLay->addWidget(new QLabel(i18n("Copy channel:"), this));
        m_copyChannel1 = new QCheckBox(i18n("1"), this);
        m_copyChannel2 = new QCheckBox(i18n("2"), this);
        m_copyChannelGroup = new QButtonGroup(this);
        m_copyChannelGroup->addButton(m_copyChannel1);
        m_copyChannelGroup->addButton(m_copyChannel2);
        m_copyChannelGroup->setExclusive(false);
        copyLay->addWidget(m_copyChannel1);
        copyLay->addWidget(m_copyChannel2);
        copyLay->addStretch(1);
        vbox->addLayout(copyLay);
        connect(m_copyChannelGroup, QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled), this, [this](QAbstractButton *but, bool) {
            if (but == m_copyChannel1) {
                QSignalBlocker bk(m_copyChannelGroup);
                m_copyChannel2->setChecked(false);
            } else {
                QSignalBlocker bk(m_copyChannelGroup);
                m_copyChannel1->setChecked(false);
            }
            if (m_copyChannel1->isChecked()) {
                m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("channelcopy from=0 to=1"));
            } else if (m_copyChannel2->isChecked()) {
                m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("channelcopy from=1 to=0"));
            } else {
                // Remove swap channels effect
                m_controller->requestRemoveStreamEffect(m_activeAudioStreams, QStringLiteral("channelcopy"));
            }
            updateStreamIcon(m_audioStreamsView->currentRow(), m_activeAudioStreams);
        });
        // Gain
        auto *gainLay = new QHBoxLayout;
        gainLay->addWidget(new QLabel(i18n("Gain:"), this));
        m_gain = new QSpinBox(this);
        m_gain->setRange(-100, 60);
        m_gain->setSuffix(i18n("dB"));
        connect(m_gain, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
            if (m_activeAudioStreams == -1) {
                // No stream selected, abort
                return;
            }
            if (value == 0) {
                // Remove effect
                m_controller->requestRemoveStreamEffect(m_activeAudioStreams, QStringLiteral("volume"));
            } else {
                m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("volume level=%1").arg(value));
            }
            updateStreamIcon(m_audioStreamsView->currentRow(), m_activeAudioStreams);
        });
        gainLay->addWidget(m_gain);
        gainLay->addStretch(1);
        vbox->addLayout(gainLay);

        vbox->addStretch(1);
        m_audioEffectGroup->setLayout(vbox);
        audioVbox->addWidget(m_audioEffectGroup);

        // Audio sync
        hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Audio sync:")));
        auto *spinSync = new QSpinBox(this);
        spinSync->setSuffix(i18n("ms"));
        spinSync->setRange(-1000, 1000);
        spinSync->setValue(qRound(1000 * m_sourceProperties->get_double("video_delay")));
        spinSync->setObjectName(QStringLiteral("video_delay"));
        if (spinSync->value() != 0) {
            m_originalProperties.insert(QStringLiteral("video_delay"), QString::number(m_sourceProperties->get_double("video_delay"), 'f'));
        }
        QObject::connect(spinSync, &QSpinBox::editingFinished, this, [this, spinSync]() {
            QMap<QString, QString> properties;
            properties.insert(QStringLiteral("video_delay"), QString::number(spinSync->value() / 1000., 'f'));
            Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
            m_originalProperties = properties;
        });
        hlay->addWidget(spinSync);
        audioVbox->addLayout(hlay);
    }

    // Force audio properties page
    QWidget *forceAudioProp = new QWidget(this);
    forceAudioProp->setLayout(audioVbox);
    forceAudioPage->setWidget(forceAudioProp);
    forceAudioPage->setWidgetResizable(true);

    return forceAudioPage;
}

void ClipPropertiesController::constructMetadataPage()
{
    m_metaPage = new QWidget(this);
    auto *box = new QVBoxLayout;
    m_metaPage->setLayout(box);

    auto *metaTree = new QTreeWidget;
    metaTree->setRootIsDecorated(true);
    metaTree->setColumnCount(2);
    metaTree->setAlternatingRowColors(true);
    metaTree->setHeaderHidden(true);
    box->addWidget(metaTree);

    slotFillMeta(metaTree);
}

void ClipPropertiesController::constructAnalysisPage()
{
    m_analysisPage = new QWidget(this);
    auto *box = new QVBoxLayout;
    m_analysisPage->setLayout(box);

    m_analysisTree = new AnalysisTree(this);
    box->addWidget(new QLabel(i18n("Analysis data")));
    box->addWidget(m_analysisTree);
    auto *bar = new QToolBar;
    bar->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete analysis"), this, SLOT(slotDeleteAnalysis()));
    bar->setWhatsThis(xi18nc("@info:whatsthis", "Deletes the data set(s)."));
    bar->addAction(QIcon::fromTheme(QStringLiteral("document-save-as")), i18n("Export analysis…"), this, SLOT(slotSaveAnalysis()));
    bar->setWhatsThis(xi18nc("@info:whatsthis", "Opens a file dialog window to export/save the analysis data."));
    bar->addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18n("Import analysis…"), this, SLOT(slotLoadAnalysis()));
    bar->setWhatsThis(xi18nc("@info:whatsthis", "Opens a file dialog window to import/load analysis data."));
    box->addWidget(bar);

    slotFillAnalysisData();
}

void ClipPropertiesController::updateStreamIcon(int row, int streamIndex)
{
    QStringList effects = m_controller->getAudioStreamEffect(streamIndex);
    QListWidgetItem *item = m_audioStreamsView->item(row);
    if (item) {
        item->setIcon(effects.isEmpty() ? QIcon() : QIcon::fromTheme(QStringLiteral("favorite")));
    }
}

void ClipPropertiesController::updateTab(int ix)
{
    KdenliveSettings::setProperties_panel_page(ix);
}

void ClipPropertiesController::slotReloadProperties()
{
    m_properties.reset(new Mlt::Properties(m_controller->properties()));
    m_sourceProperties.reset(new Mlt::Properties());
    m_controller->mirrorOriginalProperties(m_sourceProperties);
    m_clipLabel->setText(m_properties->get("kdenlive:clipname"));
    switch (m_type) {
    case ClipType::Color: {
        mlt_color color;
        m_originalProperties.insert(QStringLiteral("resource"), m_properties->get("resource"));
        m_originalProperties.insert(QStringLiteral("out"), m_properties->get("out"));
        m_originalProperties.insert(QStringLiteral("length"), m_properties->get("length"));
        Q_EMIT durationModified(m_properties->get_int("length"));
        color = m_properties->get_color("resource");
        Q_EMIT colorModified(QColor::fromRgb(color.r, color.g, color.b));
        break;
    }
    case ClipType::TextTemplate:
        m_textEdit->setPlainText(m_properties->get("templatetext"));
        break;
    case ClipType::Playlist:
    case ClipType::Image:
    case ClipType::AV:
    case ClipType::Video: {
        QString proxy = m_properties->get("kdenlive:proxy");
        if (proxy != m_originalProperties.value(QStringLiteral("kdenlive:proxy"))) {
            m_originalProperties.insert(QStringLiteral("kdenlive:proxy"), proxy);
            Q_EMIT proxyModified(proxy);
        }
        if (m_audioStreamsView && m_audioStreamsView->count() > 0) {
            int audio_ix = m_properties->get_int("audio_index");
            m_originalProperties.insert(QStringLiteral("kdenlive:active_streams"), m_properties->get("kdenlive:active_streams"));
            if (audio_ix != m_originalProperties.value(QStringLiteral("audio_index")).toInt()) {
                QSignalBlocker bk(m_audioStream);
                m_originalProperties.insert(QStringLiteral("audio_index"), QString::number(audio_ix));
            }
            QList<int> enabledStreams = m_controller->activeStreams().keys();
            qDebug() << "=== GOT ACTIVE STREAMS: " << enabledStreams;
            QSignalBlocker bk(m_audioStreamsView);
            for (int ix = 0; ix < m_audioStreamsView->count(); ix++) {
                QListWidgetItem *item = m_audioStreamsView->item(ix);
                int stream = item->data(Qt::UserRole).toInt();
                item->setCheckState(enabledStreams.contains(stream) ? Qt::Checked : Qt::Unchecked);
            }
        }
        break;
    }
    case ClipType::Timeline: {
        int tracks = m_properties->get_int("kdenlive:sequenceproperties.tracksCount");
        QList<QStringList> propertyMap;
        propertyMap.append({i18n("Tracks:"), QString::number(tracks)});
        fillProperties();
        const QUuid uuid(m_properties->get("kdenlive:uuid"));
        int timecodeOffset = pCore->currentDoc()->getSequenceProperty(uuid, "kdenlive:sequenceproperties.timecodeOffset").toInt();
        m_originalProperties.insert(QStringLiteral("kdenlive:sequenceproperties.timecodeOffset"), QString::number(timecodeOffset));
        Q_EMIT timecodeModified(timecodeOffset);
        break;
    }
    default:
        break;
    }
}

void ClipPropertiesController::slotColorModified(const QColor &newcolor)
{
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("resource"), newcolor.name(QColor::HexArgb));
    QMap<QString, QString> oldProperties;
    oldProperties.insert(QStringLiteral("resource"), m_properties->get("resource"));
    Q_EMIT updateClipProperties(m_id, oldProperties, properties);
}

void ClipPropertiesController::slotTimecodeChanged(int value)
{
    QMap<QString, QString> properties;
    auto *widget = qobject_cast<QWidget *>(sender());
    const QString objectName = widget->objectName();
    properties.insert(objectName, QString::number(value));
    Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotDurationChanged(int duration)
{
    QMap<QString, QString> properties;
    // kdenlive_length is the default duration for image / title clips
    int kdenlive_length = m_properties->time_to_frames(m_properties->get("kdenlive:duration"));
    int current_length = m_properties->get_int("length");
    if (kdenlive_length > 0) {
        // special case, image/title clips store default duration in kdenlive:duration property
        properties.insert(QStringLiteral("kdenlive:duration"), m_properties->frames_to_time(duration));
        if (duration > current_length) {
            properties.insert(QStringLiteral("length"), m_properties->frames_to_time(duration));
            properties.insert(QStringLiteral("out"), m_properties->frames_to_time(duration - 1));
        }
    } else {
        properties.insert(QStringLiteral("length"), m_properties->frames_to_time(duration));
        properties.insert(QStringLiteral("out"), m_properties->frames_to_time(duration - 1));
    }
    Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotEnableForce(int state)
{
    auto *box = qobject_cast<QCheckBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName();
    QMap<QString, QString> properties;
    if (state == Qt::Unchecked) {
        // The force property was disable, remove it / reset default if necessary
        if (param == QLatin1String("force_duration")) {
            // special case, reset original duration
            auto *timePos = findChild<TimecodeDisplay *>(param + QStringLiteral("_value"));
            timePos->setValue(m_properties->get_int("kdenlive:original_length"));
            int original = m_properties->get_int("kdenlive:original_length");
            m_properties->set("kdenlive:original_length", nullptr);
            slotDurationChanged(original);
            return;
        }
        if (param == QLatin1String("kdenlive:transparency")) {
            properties.insert(param, QString());
        } else if (param == QLatin1String("force_ar")) {
            properties.insert(QStringLiteral("force_aspect_ratio"), QString());
            // } else if (param == QLatin1String("autorotate")) {
            //     properties.insert(QStringLiteral("autorotate"), QString());
        } else {
            properties.insert(param, QString());
        }
    } else {
        // A force property was set
        if (param == QLatin1String("force_duration")) {
            int original_length = m_properties->get_int("kdenlive:original_length");
            if (original_length == 0) {
                int kdenlive_length = m_properties->time_to_frames(m_properties->get("kdenlive:duration"));
                m_properties->set("kdenlive:original_length", kdenlive_length > 0 ? m_properties->get("kdenlive:duration") : m_properties->get("length"));
            }
        } else if (param == QLatin1String("force_fps")) {
            auto *spin = findChild<QDoubleSpinBox *>(param + QStringLiteral("_value"));
            if (!spin) {
                return;
            }
            properties.insert(param, QString::number(spin->value(), 'f'));
        } else if (param == QLatin1String("threads")) {
            auto *spin = findChild<QSpinBox *>(param + QStringLiteral("_value"));
            if (!spin) {
                return;
            }
            properties.insert(param, QString::number(spin->value()));
        } else if (param == QLatin1String("force_colorspace") || param == QLatin1String("force_progressive") || param == QLatin1String("force_tff") ||
                   param == QLatin1String("color_range")) {
            auto *combo = findChild<QComboBox *>(param + QStringLiteral("_value"));
            if (!combo) {
                return;
            }
            properties.insert(param, QString::number(combo->currentData().toInt()));
            // } else if (param == QLatin1String("autorotate")) {
            //     properties.insert(QStringLiteral("autorotate"), QStringLiteral("0"));
        } else if (param == QLatin1String("force_ar")) {
            auto *spin = findChild<QSpinBox *>(QStringLiteral("force_aspect_num_value"));
            auto *spin2 = findChild<QSpinBox *>(QStringLiteral("force_aspect_den_value"));
            if ((spin == nullptr) || (spin2 == nullptr)) {
                return;
            }
            properties.insert(QStringLiteral("force_aspect_ratio"), QString::number(double(spin->value()) / spin2->value(), 'f'));
        } else if (param == QLatin1String("disable_exif")) {
            properties.insert(QStringLiteral("disable_exif"), QString::number(1));
        }
    }
    if (properties.isEmpty()) {
        return;
    }
    Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotAspectValueChanged(int)
{
    auto *spin = findChild<QSpinBox *>(QStringLiteral("force_aspect_num_value"));
    auto *spin2 = findChild<QSpinBox *>(QStringLiteral("force_aspect_den_value"));
    if ((spin == nullptr) || (spin2 == nullptr)) {
        return;
    }
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("force_aspect_ratio"), QString::number(double(spin->value()) / spin2->value(), 'f'));
    Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

QList<QStringList> ClipPropertiesController::getVideoProperties(int streamIndex)
{
    // We have a video stream
    const QString codecInfo = QStringLiteral("meta.media.%1.codec.").arg(streamIndex);
    const QString streamInfo = QStringLiteral("meta.media.%1.stream.").arg(streamIndex);

    QList<QStringList> propertyMap;

    // Video codec
    QString property = codecInfo + QStringLiteral("long_name");
    QString codec = m_sourceProperties->get(property.toUtf8().constData());
    if (!codec.isEmpty()) {
        propertyMap.append({i18n("Video codec:"), codec});
    }

    // Frame size
    int width = m_sourceProperties->get_int("meta.media.width");
    int height = m_sourceProperties->get_int("meta.media.height");
    propertyMap.append({i18n("Frame size:"), QString::number(width) + QLatin1Char('x') + QString::number(height)});

    // Frame rate
    property = streamInfo + QStringLiteral("frame_rate");
    QString fpsValue = m_sourceProperties->get(property.toUtf8().constData());
    if (!fpsValue.isEmpty()) {
        propertyMap.append({i18n("Frame rate:"), fpsValue});
    } else {
        int rate_den = m_sourceProperties->get_int("meta.media.frame_rate_den");
        if (rate_den > 0) {
            double fps = double(m_sourceProperties->get_int("meta.media.frame_rate_num")) / rate_den;
            propertyMap.append({i18n("Frame rate:"), QString::number(fps, 'f', 2)});
        }
    }

    // Video bitrate
    property = codecInfo + QStringLiteral("bit_rate");
    int bitrate = m_sourceProperties->get_int(property.toUtf8().constData()) / 1000;
    if (bitrate > 0) {
        propertyMap.append({i18n("Video bitrate:"), QString::number(bitrate) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s")});
    }

    // Scanning
    int scan = m_sourceProperties->get_int("meta.media.progressive");
    propertyMap.append({i18n("Scanning:"), (scan == 1 ? i18n("Progressive") : i18n("Interlaced"))});

    // Pixel aspect ratio
    property = codecInfo + QStringLiteral("sample_aspect_ratio");
    double par = m_sourceProperties->get_double(property.toUtf8().constData());
    if (qFuzzyIsNull(par)) {
        // Read media aspect ratio
        par = m_sourceProperties->get_double("meta.media.sample_aspect_num");
        double den = m_sourceProperties->get_double("meta.media.sample_aspect_den");
        if (den > 0) {
            par /= den;
        }
    }
    propertyMap.append({i18n("Pixel aspect ratio:"), QString::number(par, 'f', 3)});
    property = codecInfo + QStringLiteral("pix_fmt");

    // Pixel format
    propertyMap.append({i18n("Pixel format:"), m_sourceProperties->get(property.toUtf8().constData())});

    // Colorspace
    int colorspace = m_sourceProperties->get_int("meta.media.colorspace");
    propertyMap.append({i18n("Colorspace:"), ProfileRepository::getColorspaceDescription(colorspace)});

    // B frames
    int b_frames = m_sourceProperties->get_int("meta.media.has_b_frames");
    propertyMap.append({i18n("B frames:"), (b_frames == 1 ? i18n("Yes") : i18n("No"))});

    return propertyMap;
}

QList<QStringList> ClipPropertiesController::getAudioProperties(int streamIndex)
{
    const QString codecInfo = QStringLiteral("meta.media.%1.codec.").arg(streamIndex);
    QList<QStringList> propertyMap;

    // Audio codec
    QString property = codecInfo + QStringLiteral("long_name");
    QString codec = m_sourceProperties->get(property.toUtf8().constData());
    if (!codec.isEmpty()) {
        propertyMap.append({i18n("Audio codec:"), codec});
    }

    // Audio channels
    property = codecInfo + QStringLiteral("channels");
    int channels = m_sourceProperties->get_int(property.toUtf8().constData());
    propertyMap.append({i18n("Audio channels:"), QString::number(channels)});

    // Audio frequency
    property = codecInfo + QStringLiteral("sample_rate");
    int srate = m_sourceProperties->get_int(property.toUtf8().constData());
    propertyMap.append({i18n("Audio frequency:"), QString::number(srate) + QLatin1Char(' ') + i18nc("Herz", "Hz")});

    // Audio bitrate
    property = codecInfo + QStringLiteral("bit_rate");
    int bitrate = m_sourceProperties->get_int(property.toUtf8().constData()) / 1000;
    if (bitrate > 0) {
        propertyMap.append({i18n("Audio bitrate:"), QString::number(bitrate) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s")});
    }

    return propertyMap;
}

void ClipPropertiesController::fillProperties()
{
    m_clipProperties.clear();
    QList<QStringList> propertyMap;
    m_propertiesTree->clear();
    m_propertiesTree->setSortingEnabled(false);

    if (m_type == ClipType::Image || m_type == ClipType::AV || m_type == ClipType::Audio || m_type == ClipType::Video) {
        // Read File Metadata through KDE's metadata system
        if (!m_controller->hasProducerProperty(QStringLiteral("kdenlive:kextractor"))) {
            m_extractJob = QtConcurrent::run(&ClipPropertiesController::extractInfo, this, m_controller->clipUrl());
            m_watcher.setFuture(m_extractJob);
        } else {
            // we have cached metadata
            Mlt::Properties subProperties;
            subProperties.pass_values(*m_properties, "kdenlive:meta.extractor.");
            if (subProperties.count() > 0) {
                for (int i = 0; i < subProperties.count(); i++) {
                    new QTreeWidgetItem(m_propertiesTree, QStringList{subProperties.get_name(i), subProperties.get(i)});
                }
            }
        }
    }

    // Get MLT's metadata
    if (m_type == ClipType::Image) {
        int width = m_sourceProperties->get_int("meta.media.width");
        int height = m_sourceProperties->get_int("meta.media.height");
        propertyMap.append({i18n("Image size:"), QString::number(width) + QLatin1Char('x') + QString::number(height)});
    } else if (m_type == ClipType::SlideShow) {
        int ttl = m_sourceProperties->get_int("ttl");
        propertyMap.append({i18n("Image duration:"), m_properties->frames_to_time(ttl)});
        if (ttl > 0) {
            int length = m_sourceProperties->get_int("length");
            if (length == 0) {
                length = m_properties->time_to_frames(m_sourceProperties->get("length"));
            }
            int cnt = qCeil(length / ttl);
            propertyMap.append({i18n("Image count:"), QString::number(cnt)});
        }
    } else if (m_type == ClipType::AV || m_type == ClipType::Video || m_type == ClipType::Audio) {
        int vindex = m_sourceProperties->get_int("video_index");
        int default_audio = m_sourceProperties->get_int("audio_index");

        // Find maximum stream index values
        m_videoStreams.clear();
        int aStreams = m_sourceProperties->get_int("meta.media.nb_streams");
        for (int ix = 0; ix < aStreams; ++ix) {
            const QString propertName = QStringLiteral("meta.media.%1.stream.type").arg(ix);
            QString type = m_sourceProperties->get(propertName.toUtf8().constData());
            if (type == QLatin1String("video")) {
                m_videoStreams << ix;
            }
        }
        m_clipProperties.insert(QStringLiteral("default_video"), QString::number(vindex));
        m_clipProperties.insert(QStringLiteral("default_audio"), QString::number(default_audio));

        // Video streams
        propertyMap.append({i18n("Video streams:"), QString::number(m_videoStreams.count())});

        if (vindex > -1) {
            propertyMap << getVideoProperties(vindex);
        }

        // Audio streams
        propertyMap.append({i18n("Audio streams:"), QString::number(m_controller->audioStreamsCount())});

        if (default_audio > -1) {
            propertyMap << getAudioProperties(default_audio);
        }
    } else if (m_type == ClipType::Timeline) {
        int tracks = m_sourceProperties->get_int("kdenlive:sequenceproperties.tracksCount");
        propertyMap.append({i18n("Tracks:"), QString::number(tracks)});
    } else if (m_type == ClipType::Playlist) {
        // The sequence unique identifier
        QMap<QString, QString> extra = m_controller->m_extraProperties;
        QMapIterator<QString, QString> ix(extra);
        while (ix.hasNext()) {
            ix.next();
            propertyMap.append({ix.key(), ix.value()});
        }
    }

    qint64 filesize = m_sourceProperties->get_int64("kdenlive:file_size");
    if (filesize > 0) {
        QLocale locale(QLocale::system()); // use the user's locale for getting proper separators!
        propertyMap.append({i18n("File size:"), KIO::convertSize(size_t(filesize)) + QStringLiteral(" (") + locale.toString(filesize) + QLatin1Char(')')});
    }
    for (int i = 0; i < propertyMap.count(); i++) {
        auto *item = new QTreeWidgetItem(m_propertiesTree, propertyMap.at(i));
        item->setToolTip(1, propertyMap.at(i).at(1));
    }
    m_propertiesTree->setSortingEnabled(true);
    m_propertiesTree->resizeColumnToContents(0);
}

void ClipPropertiesController::extractInfo(const QString &url)
{
    KFileMetaData::ExtractorCollection metaDataCollection;
    QMimeDatabase mimeDatabase;
    QMimeType mimeType = mimeDatabase.mimeTypeForFile(url);
    QMap<QString, QString> extractionMap;
    for (KFileMetaData::Extractor *plugin : metaDataCollection.fetchExtractors(mimeType.name())) {
        ExtractionResult extractionResult(url, mimeType.name(), &extractionMap);
        plugin->extract(&extractionResult);
    }
    if (m_closing) {
        return;
    }
    QMetaObject::invokeMethod(this, "addMetadata", Q_ARG(stringMap, extractionMap));
}

void ClipPropertiesController::addMetadata(const QMap<QString, QString> meta)
{
    m_controller->setProducerProperty(QStringLiteral("kdenlive:kextractor"), 1);
    for (auto i = meta.cbegin(), end = meta.cend(); i != end; ++i) {
        qDebug() << ":::::: GOT KDENLIT_META: " << i.key() << " = " << i.value();
        new QTreeWidgetItem(m_propertiesTree, QStringList{i.key(), i.value()});
        m_controller->setProducerProperty(QStringLiteral("kdenlive:meta.extractor.%1").arg(i.key()), i.value());
    }
}

QMap<QString, QString> ClipPropertiesController::getMetadateMagicLantern()
{
    QString url = m_controller->clipUrl();
    url = url.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".LOG");
    if (!QFile::exists(url)) {
        return {};
    }

    QFile file(url);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QMap<QString, QString> metadata;

    m_controller->setProducerProperty(QStringLiteral("kdenlive:magiclantern"), 1);

    while (!file.atEnd()) {
        QString line = file.readLine().simplified();
        if (line.startsWith('#') || line.isEmpty() || !line.contains(QLatin1Char(':'))) {
            continue;
        }
        if (line.startsWith(QLatin1String("CSV data"))) {
            break;
        }
        QString key = line.section(QLatin1Char(':'), 0, 0).simplified();
        QString value = line.section(QLatin1Char(':'), 1).simplified();
        m_controller->setProducerProperty("kdenlive:meta.magiclantern." + key, value);

        metadata.insert(key, value);
    }
    return metadata;
}

QMap<QString, QString> ClipPropertiesController::getMetadataExif()
{
    QString exifToolBinary = QStandardPaths::findExecutable(QStringLiteral("exiftool"));
    if (exifToolBinary.isEmpty()) {
        return {};
    }

    QString url = m_controller->clipUrl();
    // Check for Canon THM file
    url = url.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".THM");

    QMap<QString, QString> metadata;

    if (QFile::exists(url)) {
        // Read the exif metadata embedded in the THM file
        QProcess p;
        QStringList args = {QStringLiteral("-g"), QStringLiteral("-args"), url};
        p.start(exifToolBinary, args);
        p.waitForFinished();
        QString res = p.readAllStandardOutput();
        m_controller->setProducerProperty(QStringLiteral("kdenlive:exiftool"), 1);
        QStringList list = res.split(QLatin1Char('\n'));
        for (const QString &tagline : std::as_const(list)) {
            if (tagline.startsWith(QLatin1String("-File")) || tagline.startsWith(QLatin1String("-ExifTool"))) {
                continue;
            }
            QString tag = tagline.section(QLatin1Char(':'), 1).simplified();
            if (tag.startsWith(QLatin1String("ImageWidth")) || tag.startsWith(QLatin1String("ImageHeight"))) {
                continue;
            }
            QString key = tag.section(QLatin1Char('='), 0, 0);
            QString value = tag.section(QLatin1Char('='), 1).simplified();

            if (!key.isEmpty() && !value.isEmpty()) {
                m_controller->setProducerProperty("kdenlive:meta.exiftool." + key, value);
                metadata.insert(key, value);
            }
        }
        return metadata; //TODO: this copies the current behaviour but maybe we should continue below eg. if metadate is empty yet?
    }

    if (!(m_type == ClipType::Image || m_controller->codec(false) == QLatin1String("h264"))) {
        return {};
    }

    QProcess p;
    QStringList args = {QStringLiteral("-g"), QStringLiteral("-args"), m_controller->clipUrl()};
    p.start(exifToolBinary, args);
    p.waitForFinished();
    QString res = p.readAllStandardOutput();
    if (m_type != ClipType::Image) {
        m_controller->setProducerProperty(QStringLiteral("kdenlive:exiftool"), 1);
    }
    QStringList list = res.split(QLatin1Char('\n'));
    for (const QString &tagline : std::as_const(list)) {
        if (m_type != ClipType::Image && !tagline.startsWith(QLatin1String("-H264"))) {
            continue;
        }
        QString tag = tagline.section(QLatin1Char(':'), 1);
        if (tag.startsWith(QLatin1String("ImageWidth")) || tag.startsWith(QLatin1String("ImageHeight"))) {
            continue;
        }
        QString key = tag.section(QLatin1Char('='), 0, 0);
        QString value = tag.section(QLatin1Char('='), 1).simplified();

        if (m_type != ClipType::Image) {
            // Do not store image exif metadata in project file, would be too much noise
            m_controller->setProducerProperty("kdenlive:meta.exiftool." + key, value);
        }
        metadata.insert(key, value);
    }

    return metadata;
}

void ClipPropertiesController::slotFillMeta(QTreeWidget *tree)
{
    tree->clear();
    if (m_type != ClipType::AV && m_type != ClipType::Video && m_type != ClipType::Image) {
        // Currently, we only use exiftool on video files
        return;
    }

    // Exif Metadata
    QMap<QString, QString> exifMetadata;
    int exifUsed = m_controller->getProducerIntProperty(QStringLiteral("kdenlive:exiftool"));
    if (exifUsed == 1) {
        // we have cached metadata
        Mlt::Properties subProperties;
        subProperties.pass_values(*m_properties, "kdenlive:meta.exiftool.");
        if (subProperties.count() > 0) {
            for (int i = 0; i < subProperties.count(); i++) {
                exifMetadata.insert(subProperties.get_name(i), subProperties.get(i));
            }
        }
    } else if (KdenliveSettings::use_exiftool()) {
        exifMetadata = getMetadataExif();
    }

    if (!exifMetadata.isEmpty()) {
        // Parent tree item
        QTreeWidgetItem *exif = new QTreeWidgetItem(tree, {i18n("Exif"), QString()});
        exif->setExpanded(true);

        // Child tree items
        QMapIterator<QString, QString> i(exifMetadata);
        while (i.hasNext()) {
            i.next();
            new QTreeWidgetItem(exif, { i.key(), i.value() });
        }
    }

    // Magic Lantern Metadata
    QMap<QString, QString> magicLanternMetadata;
    int magic = m_controller->getProducerIntProperty(QStringLiteral("kdenlive:magiclantern"));
    if (magic == 1) {
        // We have cached metadata
        Mlt::Properties subProperties;
        subProperties.pass_values(*m_properties, "kdenlive:meta.magiclantern.");
        for (int i = 0; i < subProperties.count(); i++) {
            magicLanternMetadata.insert(subProperties.get_name(i), subProperties.get(i));
        }
    } else if (m_type != ClipType::Image && KdenliveSettings::use_magicLantern()) {
        magicLanternMetadata = getMetadateMagicLantern();
    }

    if (!magicLanternMetadata.isEmpty()) {
        // Parent tree item
        QTreeWidgetItem *magicL = new QTreeWidgetItem(tree, { i18n("Magic Lantern"), QString() });
        QIcon icon(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("meta_magiclantern.png")));
        magicL->setIcon(0, icon);
        magicL->setExpanded(true);

        // Child tree items
        QMapIterator<QString, QString> i(magicLanternMetadata);
        while (i.hasNext()) {
            i.next();
            new QTreeWidgetItem(magicL, { i.key(), i.value() });
        }
    }

    // Final touches
    tree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotFillAnalysisData()
{
    m_analysisTree->clear();
    Mlt::Properties subProperties;
    subProperties.pass_values(*m_properties, "kdenlive:clipanalysis.");
    if (subProperties.count() > 0) {
        for (int i = 0; i < subProperties.count(); i++) {
            new QTreeWidgetItem(m_analysisTree, {subProperties.get_name(i), subProperties.get(i)});
        }
    }
    m_analysisTree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotDeleteAnalysis()
{
    QTreeWidgetItem *current = m_analysisTree->currentItem();
    if (!current) {
        return;
    }
    Q_EMIT editAnalysis(m_id, "kdenlive:clipanalysis." + current->text(0), QString());
}

void ClipPropertiesController::slotSaveAnalysis()
{
    const QString url = QFileDialog::getSaveFileName(this, i18nc("@title:window", "Save Analysis Data"), QFileInfo(m_controller->clipUrl()).absolutePath(),
                                                     i18n("Text File (*.txt)"));
    if (url.isEmpty()) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(url, KConfig::SimpleConfig);
    KConfigGroup analysisConfig(config, "Analysis");
    QTreeWidgetItem *current = m_analysisTree->currentItem();
    analysisConfig.writeEntry(current->text(0), current->text(1));
}

void ClipPropertiesController::slotLoadAnalysis()
{
    const QString url = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Open Analysis Data"), QFileInfo(m_controller->clipUrl()).absolutePath(),
                                                     i18n("Text File (*.txt)"));
    if (url.isEmpty()) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(url, KConfig::SimpleConfig);
    KConfigGroup transConfig(config, "Analysis");
    // read the entries
    QMap<QString, QString> profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        Q_EMIT editAnalysis(m_id, "kdenlive:clipanalysis." + i.key(), i.value());
    }
}

void ClipPropertiesController::slotTextChanged()
{
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("templatetext"), m_textEdit->toPlainText());
    Q_EMIT updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::activatePage(int ix)
{
    m_tabWidget->setCurrentIndex(ix);
}

void ClipPropertiesController::updateStreamInfo(int streamIndex)
{
    QStringList effects = m_controller->getAudioStreamEffect(m_activeAudioStreams);
    QListWidgetItem *item = nullptr;
    for (int ix = 0; ix < m_audioStreamsView->count(); ix++) {
        QListWidgetItem *it = m_audioStreamsView->item(ix);
        int stream = it->data(Qt::UserRole).toInt();
        if (stream == m_activeAudioStreams) {
            item = it;
            break;
        }
    }
    if (item) {
        item->setIcon(effects.isEmpty() ? QIcon() : QIcon::fromTheme(QStringLiteral("favorite")));
    }
    if (streamIndex == m_activeAudioStreams) {
        QSignalBlocker bk(m_swapChannels);
        QSignalBlocker bk1(m_copyChannelGroup);
        QSignalBlocker bk2(m_normalize);
        m_swapChannels->setChecked(effects.contains(QLatin1String("channelswap")));
        m_copyChannel1->setChecked(effects.contains(QStringLiteral("channelcopy from=0 to=1")));
        m_copyChannel2->setChecked(effects.contains(QStringLiteral("channelcopy from=1 to=0")));
        m_normalize->setChecked(effects.contains(QStringLiteral("dynamic_loudness")));
        int gain = 0;
        for (const QString &st : std::as_const(effects)) {
            if (st.startsWith(QLatin1String("volume "))) {
                QSignalBlocker bk3(m_gain);
                gain = st.section(QLatin1Char('='), 1).toInt();
                break;
            }
        }
        QSignalBlocker bk3(m_gain);
        m_gain->setValue(gain);
    }
}
