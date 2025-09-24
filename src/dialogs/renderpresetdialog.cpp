/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderpresetdialog.h"

#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitormanager.h"
#include "profiles/profilemodel.hpp"
#include "renderpresets/renderpresetmodel.hpp"
#include "renderpresets/renderpresetrepository.hpp"

#include <KMessageBox>
#include <QPushButton>
#include <QScrollBar>

// TODO replace this by std::gcd ones why require C++17 or greater
static int gcd(int a, int b)
{
    for (;;) {
        if (a == 0) return b;
        b %= a;
        if (b == 0) return a;
        a %= b;
    }
}

RenderPresetDialog::RenderPresetDialog(QWidget *parent, RenderPresetModel *preset, Mode mode)
    : QDialog(parent)
    , m_saveName(preset ? preset->name() : "")
    , m_monitor(nullptr)
    , m_fixedResRatio(1.)
    , m_manualPreset(false)
{
    setupUi(this);
    if (preset) {
        m_manualPreset = preset->isManual();
    }
    m_uiParams.append({QStringLiteral("f"),
                       QStringLiteral("acodec"),
                       QStringLiteral("vcodec"),
                       QStringLiteral("width"),
                       QStringLiteral("height"),
                       QStringLiteral("s"),
                       QStringLiteral("r"),
                       QStringLiteral("frame_rate_num"),
                       QStringLiteral("frame_rate_den"),
                       QStringLiteral("crf"),
                       QStringLiteral("color_range"),
                       QStringLiteral("vb"),
                       QStringLiteral("vminrate"),
                       QStringLiteral("vmaxrate"),
                       QStringLiteral("vglobal_quality"),
                       QStringLiteral("vq"),
                       QStringLiteral("rc"),
                       QStringLiteral("g"),
                       QStringLiteral("bf"),
                       QStringLiteral("progressive"),
                       QStringLiteral("top_field_first"),
                       QStringLiteral("qscale"),
                       QStringLiteral("vbufsize"),
                       QStringLiteral("qmin"),
                       QStringLiteral("qp_i"),
                       QStringLiteral("qp_p"),
                       QStringLiteral("qp_b"),
                       QStringLiteral("ab"),
                       QStringLiteral("aq"),
                       QStringLiteral("compression_level"),
                       QStringLiteral("vbr"),
                       QStringLiteral("ar"),
                       QStringLiteral("display_aspect_num"),
                       QStringLiteral("display_aspect_den"),
                       QStringLiteral("sample_aspect_num"),
                       QStringLiteral("sample_aspect_den"),
                       QStringLiteral("aspect"),
                       QStringLiteral("sc_threshold"),
                       QStringLiteral("strict_gop"),
                       QStringLiteral("keyint_min"),
                       QStringLiteral("channels")});

    formatCombo->addItems(RenderPresetRepository::supportedFormats());
    vCodecCombo->addItems(RenderPresetRepository::vcodecs());
    aCodecCombo->addItems(RenderPresetRepository::acodecs());

    aRateControlCombo->addItem(i18n("(Not set)"));
    aRateControlCombo->addItem(i18n("Average Bitrate"));
    aRateControlCombo->addItem(i18n("CBR – Constant Bitrate"));
    aRateControlCombo->addItem(i18n("VBR – Variable Bitrate"));

    audioChannels->addItem(i18n("1 (mono)"), 1);
    audioChannels->addItem(i18n("2 (stereo)"), 2);
    audioChannels->addItem(i18n("4"), 4);
    audioChannels->addItem(i18n("6"), 6);

    QValidator *validator = new QIntValidator(this);
    audioSampleRate->setValidator(validator);

    // Add some common pixel aspect ratios:
    // The following code works, because setting a yet
    // unknown ratio will add it to the combo box.
    setPixelAspectRatio(1, 1);
    setPixelAspectRatio(10, 11);
    setPixelAspectRatio(12, 11);
    setPixelAspectRatio(59, 54);
    setPixelAspectRatio(4, 3);
    setPixelAspectRatio(64, 45);
    setPixelAspectRatio(11, 9);
    setPixelAspectRatio(118, 81);

    //
    QPushButton *helpButton = buttonBox->button(QDialogButtonBox::Help);
    helpButton->setText(QString());
    helpButton->setIcon(QIcon::fromTheme(QStringLiteral("settings-configure")));
    helpButton->setToolTip(i18n("Edit Render Preset"));
    helpButton->setCheckable(true);
    helpButton->setChecked(KdenliveSettings::showRenderTextParameters());
    parameters->setVisible(KdenliveSettings::showRenderTextParameters());
    connect(helpButton, &QPushButton::toggled, this, [this](bool checked) {
        parameters->setVisible(checked);
        KdenliveSettings::setShowRenderTextParameters(checked);
    });

    connect(vRateControlCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index) {
        switch (index) {
        case RenderPresetParams::RateControl::Average:
            default_vbitrate->setEnabled(true);
            default_vquality->setEnabled(false);
            vquality_label->setEnabled(false);
            vBuffer->setEnabled(false);
            break;
        case RenderPresetParams::RateControl::Constant:
            default_vbitrate->setEnabled(true);
            default_vquality->setEnabled(false);
            vquality_label->setEnabled(false);
            vBuffer->setEnabled(true);
            break;
        case RenderPresetParams::RateControl::Constrained:
            default_vbitrate->setEnabled(true);
            default_vquality->setEnabled(true);
            vquality_label->setEnabled(true);
            vBuffer->setEnabled(true);
            break;
        case RenderPresetParams::RateControl::Quality:
            default_vbitrate->setEnabled(false);
            default_vquality->setEnabled(true);
            vquality_label->setEnabled(true);
            vBuffer->setEnabled(false);
            break;
        default:
            default_vbitrate->setEnabled(false);
            default_vquality->setEnabled(false);
            vquality_label->setEnabled(false);
            vBuffer->setEnabled(false);
            break;
        };
        slotUpdateParams();
    });

    vRateControlCombo->addItem(i18n("(Not set)"));
    vRateControlCombo->addItem(i18n("Average Bitrate"));
    vRateControlCombo->addItem(i18n("CBR – Constant Bitrate"));
    vRateControlCombo->addItem(i18n("VBR – Variable Bitrate"));
    vRateControlCombo->addItem(i18n("Constrained VBR"));

    connect(scanningCombo, &QComboBox::currentTextChanged, this, [&]() {
        fieldOrderCombo->setEnabled(scanningCombo->currentIndex() != 1);
        cField->setEnabled(scanningCombo->currentIndex() != 1);
        slotUpdateParams();
    });
    connect(gopSpinner, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value) {
        fixedGop->setEnabled(value > 1);
        bFramesSpinner->setEnabled(value > 1);
        bFramesLabel->setEnabled(value > 1);
        if (value <= 1) {
            fixedGop->blockSignals(true);
            fixedGop->setChecked(false);
            fixedGop->blockSignals(false);
            bFramesSpinner->blockSignals(true);
            bFramesSpinner->setValue(-1);
            bFramesSpinner->blockSignals(false);
        } else {
            bFramesSpinner->setMaximum(value - 1);
        }
        slotUpdateParams();
    });

    groupName->addItems(RenderPresetRepository::get()->groupNames());

    std::unique_ptr<ProfileModel> &projectProfile = pCore->getCurrentProfile();
    int parNum = projectProfile->sample_aspect_num();
    int parDen = projectProfile->sample_aspect_den();
    connect(button_manual, &QPushButton::clicked, this, [this, helpButton]() {
        if (m_manualPreset) {
            return;
        }
        tabWidget->setVisible(false);
        label_container->setEnabled(false);
        formatCombo->setEnabled(false);
        parameters->setVisible(true);
        parameters->setReadOnly(false);
        helpButton->setEnabled(false);
        m_manualPreset = true;
    });
    if (preset) {
        groupName->setCurrentText(preset->groupName());
        if (mode != Mode::New) {
            preset_name->setText(preset->name());
        }
        preset_name->setFocus();
        if (m_manualPreset) {
            tabWidget->setVisible(false);
            label_container->setEnabled(false);
            formatCombo->setEnabled(false);
            parameters->setVisible(true);
            parameters->setPlainText(preset->params().toString());
            parameters->setReadOnly(false);
            helpButton->setEnabled(false);
            preset_extension->setText(preset->extension());
        } else {
            formatCombo->setCurrentText(preset->getParam(QStringLiteral("f")));
            preset_extension->setText(preset->extension());
            QString width = preset->getParam(QStringLiteral("width"));
            QString height = preset->getParam(QStringLiteral("height"));
            QString size = preset->getParam(QStringLiteral("s"));

            if (!(width.isEmpty() && height.isEmpty())) {
                resWidth->setValue(width.toInt());
                resHeight->setValue(height.toInt());
                cResolution->setChecked(true);
            } else if (!size.isEmpty()) {
                QStringList list = size.split(QStringLiteral("x"));
                if (list.count() == 2) {
                    resWidth->setValue(list.at(0).toInt());
                    resHeight->setValue(list.at(1).toInt());
                    cResolution->setChecked(true);
                }
            } else {
                resWidth->setValue(projectProfile->width());
                resHeight->setValue(projectProfile->height());
            }

            // video tab
            if (preset->hasParam(QStringLiteral("vbufsize"))) {
                bool ok = false;
                int vbufsize = preset->getParam(QStringLiteral("vbufsize")).toInt(&ok);
                if (ok) {
                    vBuffer->setValue(vbufsize / 1024 / 8);
                }
            }
            QString vqParam = preset->getParam(QStringLiteral("vq"));
            QString vbParam = preset->getParam(QStringLiteral("vb"));
            if (vqParam.isEmpty()) {
                vqParam = preset->getParam(QStringLiteral("vqp"));
            }
            if (vqParam.isEmpty()) {
                vqParam = preset->getParam(QStringLiteral("vglobal_quality"));
            }
            if (vqParam.isEmpty()) {
                vqParam = preset->getParam(QStringLiteral("qscale"));
            }
            if (vqParam.isEmpty()) {
                vqParam = preset->getParam(QStringLiteral("crf"));
            }
            if (vqParam == QStringLiteral("%quality")) {
                default_vquality->setValue(preset->defaultVQuality().toInt());
            } else {
                default_vquality->setValue(vqParam.toInt());
            }

            vRateControlCombo->setCurrentIndex(preset->params().videoRateControl());

            if (vbParam.isEmpty()) {
                vbParam = preset->getParam(QStringLiteral("vmaxrate"));
            }
            if (vbParam.contains(QStringLiteral("%bitrate"))) {
                default_vbitrate->setValue(preset->defaultVBitrate().toInt());
            } else if (!vbParam.isEmpty()) {
                default_vbitrate->setValue(vbParam.replace('k', "").replace('M', "000").toInt());
            }

            QString sampAspNum = preset->getParam(QStringLiteral("sample_aspect_num"));
            QString sampAspDen = preset->getParam(QStringLiteral("sample_aspect_den"));
            QString sampAsp = preset->getParam(QStringLiteral("aspect"));

            if (!(sampAspNum.isEmpty() && sampAspDen.isEmpty())) {
                parNum = sampAspNum.toInt();
                parDen = sampAspDen.toInt();
                cPar->setChecked(true);
            } else if (!sampAsp.isEmpty()) {
                QStringList list = sampAsp.split(QStringLiteral("/"));
                if (list.count() == 2) {
                    parNum = list.at(0).toInt();
                    parDen = list.at(1).toInt();
                    cPar->setChecked(true);
                }
            }

            if (preset->hasParam(QStringLiteral("display_aspect_num")) && preset->hasParam(QStringLiteral("display_aspect_den"))) {
                displayAspectNum->setValue(preset->getParam(QStringLiteral("display_aspect_num")).toInt());
                displayAspectDen->setValue(preset->getParam(QStringLiteral("display_aspect_den")).toInt());
                cDar->setChecked(true);
            } else {
                displayAspectNum->setValue(projectProfile->display_aspect_num());
                displayAspectDen->setValue(projectProfile->display_aspect_den());
            }

            vCodecCombo->setCurrentText(preset->getParam(QStringLiteral("vcodec")));
            if (preset->hasParam(QStringLiteral("r"))) {
                cFps->setChecked(true);
                double val = preset->getParam(QStringLiteral("r")).toDouble();
                if (val == 23.98 || val == 23.976 || val == (24000 / 1001)) {
                    framerateNum->setValue(24000);
                    framerateDen->setValue(1001);
                } else if (val == 29.97 || val == (30000 / 1001)) {
                    framerateNum->setValue(30000);
                    framerateDen->setValue(1001);
                } else if (val == 47.95 || val == (48000 / 1001)) {
                    framerateNum->setValue(48000);
                    framerateDen->setValue(1001);
                } else if (val == 59.94 || val == (60000 / 1001)) {
                    framerateNum->setValue(60000);
                    framerateDen->setValue(1001);
                } else {
                    framerateNum->setValue(qRound(val * 1000));
                    framerateDen->setValue(1000);
                }
            } else if (preset->hasParam(QStringLiteral("frame_rate_num")) && preset->hasParam(QStringLiteral("frame_rate_den"))) {
                framerateNum->setValue(preset->getParam(QStringLiteral("frame_rate_num")).toInt());
                framerateDen->setValue(preset->getParam(QStringLiteral("frame_rate_den")).toInt());
                cFps->setChecked(true);
            } else {
                framerateNum->setValue(projectProfile->frame_rate_num());
                framerateDen->setValue(projectProfile->frame_rate_den());
            }
            if (preset->hasParam(QStringLiteral("progressive"))) {
                scanningCombo->setCurrentIndex(preset->getParam(QStringLiteral("progressive")).toInt());
                cScanning->setChecked(true);
            }
            if (preset->hasParam(QStringLiteral("top_field_first"))) {
                fieldOrderCombo->setCurrentIndex(preset->getParam(QStringLiteral("top_field_first")).toInt());
                cField->setChecked(true);
            }
            int gopVal = -1;
            if (preset->hasParam(QStringLiteral("g"))) {
                gopVal = preset->getParam(QStringLiteral("g")).toInt();
                gopSpinner->setValue(gopVal);
            }
            if (preset->hasParam(QStringLiteral("sc_threshold")))
                fixedGop->setChecked(preset->getParam(QStringLiteral("sc_threshold")).toInt() == 0);
            else if (preset->hasParam(QStringLiteral("keyint_min")))
                fixedGop->setChecked(preset->getParam(QStringLiteral("keyint_min")).toInt() == gopVal);
            else if (preset->hasParam(QStringLiteral("strict_gop"))) {
                fixedGop->setChecked(preset->getParam(QStringLiteral("strict_gop")).toInt() == 1);
            } else {
                fixedGop->setChecked(false);
            }
            bFramesSpinner->setValue(preset->getParam(QStringLiteral("bf")).toInt());

            // audio tab
            if (preset->hasParam(QStringLiteral("channels"))) {
                int ix = audioChannels->findData(preset->getParam(QStringLiteral("channels")).toInt());
                audioChannels->setCurrentIndex(ix);
                cChannels->setChecked(true);
            } else {
                int ix = audioChannels->findData(2);
                audioChannels->setCurrentIndex(ix);
            }
            aRateControlCombo->setCurrentIndex(preset->audioRateControl());

            if (preset->hasParam(QStringLiteral("channels"))) {
                audioSampleRate->setCurrentText(preset->getParam(QStringLiteral("ar")));
                cSampleR->setChecked(true);
            } else {
                audioSampleRate->setCurrentText(QStringLiteral("44100"));
            }

            QString aqParam = preset->getParam(QStringLiteral("aq"));
            if (aqParam.isEmpty()) {
                aqParam = preset->getParam(QStringLiteral("compression_level"));
            }
            if (aqParam.contains(QStringLiteral("%audioquality"))) {
                aQuality->setValue(preset->defaultAQuality().toInt());
            } else {
                aQuality->setValue(aqParam.toInt());
            }
            QString abParam = preset->getParam(QStringLiteral("ab"));
            if (abParam.contains(QStringLiteral("%audiobitrate"))) {
                aBitrate->setValue(preset->defaultABitrate().toInt());
            } else {
                aBitrate->setValue(abParam.replace('k', "").replace('M', "000").toInt());
            }

            aCodecCombo->setCurrentText(preset->getParam(QStringLiteral("acodec")));

            // general tab
            speeds_list->setText(preset->speeds().join('\n'));
            additionalParams->setPlainText(preset->params(m_uiParams).toString());
        }
    } else {
        resHeight->setValue(projectProfile->height());
        resWidth->setValue(projectProfile->width());
        framerateNum->setValue(projectProfile->frame_rate_num());
        framerateDen->setValue(projectProfile->frame_rate_den());
        parNum = projectProfile->sample_aspect_num();
        parDen = projectProfile->sample_aspect_den();
        displayAspectNum->setValue(projectProfile->display_aspect_num());
        displayAspectDen->setValue(projectProfile->display_aspect_den());
        scanningCombo->setCurrentIndex(projectProfile->progressive() ? 1 : 0);
    }

    setPixelAspectRatio(parNum, parDen);

    if (groupName->currentText().isEmpty()) {
        groupName->setCurrentText(i18nc("Group Name", "Custom"));
    }

    if (mode == Mode::Edit) {
        setWindowTitle(i18n("Edit Render Preset"));
    }

    connect(preset_name, &QLineEdit::textChanged, this, [&](const QString &text) { buttonBox->button(QDialogButtonBox::Ok)->setDisabled(text.isEmpty()); });

    connect(buttonBox, &QDialogButtonBox::accepted, this, [&, preset, mode]() {
        QString oldName;
        if (preset) {
            oldName = preset->name();
        }
        QString newPresetName = preset_name->text().simplified();
        if (newPresetName.isEmpty()) {
            KMessageBox::error(this, i18n("The preset name can't be empty"));
            return;
        }
        QString newGroupName = groupName->currentText().simplified();
        if (newGroupName.isEmpty()) {
            newGroupName = i18nc("Group Name", "Custom");
        }
        QString speeds_list_str = speeds_list->toPlainText().replace('\n', ';').simplified();

        QString qualities_str{""};
        // If the base preset had a custom quality scale, get it
        if (preset) {
            qualities_str = preset->videoQualities().join(',');
        }

        std::unique_ptr<RenderPresetModel> newPreset(
            new RenderPresetModel(newPresetName, newGroupName, parameters->toPlainText().simplified(), preset_extension->text().simplified(),
                                  QString::number(default_vbitrate->value()), QString::number(default_vquality->value()), qualities_str,
                                  QString::number(aBitrate->value()), QString::number(aQuality->value()), speeds_list_str, m_manualPreset));

        m_saveName = RenderPresetRepository::get()->savePreset(newPreset.get(), mode == Mode::Edit);
        if ((mode == Mode::Edit) && !m_saveName.isEmpty() && (oldName != m_saveName)) {
            RenderPresetRepository::get()->deletePreset(oldName);
        }
        accept();
    });

    connect(formatCombo, &QComboBox::currentTextChanged, this, [this](const QString &format) {
        if (format == QLatin1String("matroska")) {
            preset_extension->setText(QStringLiteral("mkv"));
        } else {
            preset_extension->setText(format);
        }
        slotUpdateParams();
    });
    connect(vCodecCombo, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);

    connect(fieldOrderCombo, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(aCodecCombo, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(linkResoultion, &QToolButton::clicked, this, [&]() { m_fixedResRatio = double(resWidth->value()) / double(resHeight->value()); });
    connect(resWidth, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value) {
        if (linkResoultion->isChecked()) {
            resHeight->blockSignals(true);
            resHeight->setValue(qRound(value / m_fixedResRatio));
            resHeight->blockSignals(false);
        }
        updateDisplayAspectRatio();
    });
    connect(resHeight, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value) {
        if (linkResoultion->isChecked()) {
            resWidth->blockSignals(true);
            resWidth->setValue(qRound(value * m_fixedResRatio));
            resWidth->blockSignals(false);
        }
        updateDisplayAspectRatio();
    });
    connect(parCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &RenderPresetDialog::updateDisplayAspectRatio);
    auto update_par = [&]() {
        int parNum = displayAspectNum->value() * resHeight->value();
        int parDen = displayAspectDen->value() * resWidth->value();
        setPixelAspectRatio(parNum, parDen);
        slotUpdateParams();
    };
    connect(displayAspectNum, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, update_par);
    connect(displayAspectDen, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, update_par);
    connect(framerateNum, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value) {
        if (value == 0) {
            framerateDen->blockSignals(true);
            framerateDen->setValue(0);
            framerateDen->blockSignals(false);
        }
        slotUpdateParams();
    });
    connect(framerateDen, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value) {
        if (value == 0) {
            framerateNum->blockSignals(true);
            framerateNum->setValue(0);
            framerateNum->blockSignals(false);
        }
        slotUpdateParams();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(fixedGop, &QCheckBox::checkStateChanged, this, &RenderPresetDialog::slotUpdateParams);
#else
    connect(fixedGop, &QCheckBox::stateChanged, this, &RenderPresetDialog::slotUpdateParams);
#endif
    connect(bFramesSpinner, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(additionalParams, &QPlainTextEdit::textChanged, this, &RenderPresetDialog::slotUpdateParams);

    connect(aRateControlCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(aBitrate, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(aQuality, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(audioChannels, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(audioSampleRate, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);

    linkResoultion->setChecked(true);
    slotUpdateParams();
    connect(cResolution, &QCheckBox::toggled, this, &RenderPresetDialog::slotUpdateParams);
    connect(cPar, &QCheckBox::toggled, this, &RenderPresetDialog::slotUpdateParams);
    connect(cDar, &QCheckBox::toggled, this, &RenderPresetDialog::slotUpdateParams);
    connect(cFps, &QCheckBox::toggled, this, &RenderPresetDialog::slotUpdateParams);
    connect(cScanning, &QCheckBox::toggled, this, &RenderPresetDialog::slotUpdateParams);
    connect(cField, &QCheckBox::toggled, this, &RenderPresetDialog::slotUpdateParams);
    connect(cChannels, &QCheckBox::toggled, this, &RenderPresetDialog::slotUpdateParams);
    connect(cSampleR, &QCheckBox::toggled, this, &RenderPresetDialog::slotUpdateParams);

    const QList<QSpinBox *> spins = findChildren<QSpinBox *>();
    for (QSpinBox *sp : spins) {
        sp->installEventFilter(this);
        sp->setFocusPolicy(Qt::StrongFocus);
    }
    const QList<QComboBox *> combos = findChildren<QComboBox *>();
    for (QComboBox *sp : combos) {
        sp->installEventFilter(this);
        sp->setFocusPolicy(Qt::StrongFocus);
    }

    // TODO
    if (false && m_monitor == nullptr) {
        // QString profile = DvdWizardVob::getDvdProfile(format);
        /*m_monitor = new Monitor(Kdenlive::RenderMonitor, pCore->monitorManager(), this);
        m_monitor->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        mainBox->addWidget(m_monitor);
        pCore->monitorManager()->appendMonitor(m_monitor);
        // m_monitor->setCustomProfile(profile, m_tc);
        pCore->monitorManager()->activateMonitor(Kdenlive::RenderMonitor);
        m_monitor->start();*/
    }
}

RenderPresetDialog::~RenderPresetDialog()
{
    if (m_monitor) {
        m_monitor->stop();
        pCore->monitorManager()->removeMonitor(m_monitor);
        delete m_monitor;
    }
}

bool RenderPresetDialog::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Wheel && (qobject_cast<QComboBox *>(o) || qobject_cast<QAbstractSpinBox *>(o))) {
        if (scrollArea->verticalScrollBar()->isVisible() && !qobject_cast<QWidget *>(o)->hasFocus()) {
            e->ignore();
            return true;
        } else {
            e->accept();
            return false;
        }
    }
    return QWidget::eventFilter(o, e);
}

void RenderPresetDialog::slotUpdateParams()
{
    if (m_manualPreset) {
        return;
    }
    QStringList params;
    QString vcodec = vCodecCombo->currentText();
    params.append(QStringLiteral("f=%1").arg(formatCombo->currentText()));
    // video tab
    params.append(QStringLiteral("vcodec=%1").arg(vcodec));
    if (cResolution->isChecked()) {
        if (resWidth->value() > 0) {
            params.append(QStringLiteral("width=%1").arg(resWidth->value()));
        }
        if (resHeight->value() > 0) {
            params.append(QStringLiteral("height=%1").arg(resHeight->value()));
        }
    }
    if (cPar->isChecked()) {
        QStringList par = parCombo->currentData().toString().split(QStringLiteral(":"));
        if (par.length() >= 2 && par.at(0).toInt() > 0 && par.at(1).toInt() > 0) {
            params.append(QStringLiteral("sample_aspect_num=%1 sample_aspect_den=%2").arg(par.at(0).toInt()).arg(par.at(1).toInt()));
        }
    }
    if (cDar->isChecked() && displayAspectNum->value() > 0 && displayAspectDen->value() > 0) {
        params.append(QStringLiteral("display_aspect_num=%1 display_aspect_den=%2").arg(displayAspectNum->value()).arg(displayAspectDen->value()));
    }

    if (cFps->isChecked() && framerateNum->value() > 0 && framerateDen->value() > 0) {
        params.append(QStringLiteral("frame_rate_num=%1").arg(framerateNum->value()));
        params.append(QStringLiteral("frame_rate_den=%1").arg(framerateDen->value()));
        frameRateDisplay->setText(QString::number(double(framerateNum->value()) / double(framerateDen->value()), 'g', 10));
        frameRateDisplay->show();
    } else {
        frameRateDisplay->hide();
    }

    // Adjust scanning
    if (cScanning->isChecked()) {
        params.append(QStringLiteral("progressive=%1").arg(scanningCombo->currentIndex()));
        if (cField->isChecked() && scanningCombo->currentIndex() == 0) {
            params.append(QStringLiteral("top_field_first=%1").arg(fieldOrderCombo->currentIndex()));
        }
    }
    if (vcodec == "libx265") {
        // Most x265 parameters must be supplied through x265-params.
        switch (vRateControlCombo->currentIndex()) {
        case RenderPresetParams::RateControl::Average:
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            break;
        case RenderPresetParams::RateControl::Constant: {
            // x265 does not expect bitrate suffixes and requires Kb/s
            params.append(QStringLiteral("vb=%bitrate"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
        }
        case RenderPresetParams::RateControl::Quality: {
            params.append(QStringLiteral("crf=%quality"));
            break;
        }
        case RenderPresetParams::RateControl::Constrained: {
            params.append(QStringLiteral("crf=%quality"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            break;
        }
        }
        if (fixedGop->isEnabled() && fixedGop->isChecked()) {
            params.append(QStringLiteral("sc_threshold=0"));
        }
    } else if (vcodec.contains("nvenc")) {
        switch (vRateControlCombo->currentIndex()) {
        case RenderPresetParams::RateControl::Average:
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            break;
        case RenderPresetParams::RateControl::Constant: {
            params.append(QStringLiteral("cbr=1 "));
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            params.append(QStringLiteral("vminrate=%bitrate+'k'"));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
        }
        case RenderPresetParams::RateControl::Quality: {
            params.append(QStringLiteral("rc=constqp"));
            params.append(QStringLiteral("vqp=%quality"));
            params.append(QStringLiteral("vq=%quality"));
            break;
        }
        case RenderPresetParams::RateControl::Constrained: {
            params.append(QStringLiteral("qmin=%quality"));
            params.append(QStringLiteral("vb=%cvbr")); // setIfNotSet(p, "vb", qRound(cvbr));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
        }
        }
        /*if (ui->dualPassCheckbox->isChecked())
            setIfNotSet(p, "v2pass", 1);
        */
        if (fixedGop->isEnabled() && fixedGop->isChecked()) {
            params.append(QStringLiteral("sc_threshold=0 strict_gop=1"));
        }
    } else if (vcodec.endsWith("_amf")) {
        switch (vRateControlCombo->currentIndex()) {
        case RenderPresetParams::RateControl::Average:
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            break;
        case RenderPresetParams::RateControl::Constant: {
            params.append(QStringLiteral("rc=cbr "));
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            params.append(QStringLiteral("vminrate=%bitrate+'k'"));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1 ").arg(vBuffer->value() * 8 * 1024));
            break;
        }
        case RenderPresetParams::RateControl::Quality: {
            params.append(QStringLiteral("rc=cqp"));
            params.append(QStringLiteral("qp_i=%quality"));
            params.append(QStringLiteral("qp_p=%quality"));
            params.append(QStringLiteral("qp_b=%quality"));
            params.append(QStringLiteral("vq=%quality"));
            break;
        }
        case RenderPresetParams::RateControl::Constrained: {
            params.append(QStringLiteral("rc=vbr_peak"));
            params.append(QStringLiteral("qmin=%quality"));
            params.append(QStringLiteral("vb=%cvbr")); // setIfNotSet(p, "vb", qRound(cvbr));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
        }
        }
        /*if (ui->dualPassCheckbox->isChecked())
            setIfNotSet(p, "v2pass", 1);
            */
        if (fixedGop->isEnabled() && fixedGop->isChecked()) {
            params.append(QStringLiteral("sc_threshold=0 strict_gop=1"));
        }
    } else {
        switch (vRateControlCombo->currentIndex()) {
        case RenderPresetParams::RateControl::Average:
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            break;
        case RenderPresetParams::RateControl::Constant: {
            // const QString& b = ui->videoBitrateCombo->currentText();
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            params.append(QStringLiteral("vminrate=%bitrate+'k'"));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
        }
        case RenderPresetParams::RateControl::Quality: {
            if (vcodec.startsWith("libx264") || vcodec.startsWith("libsvtav1")) {
                params.append(QStringLiteral("crf=%quality"));
            } else if (vcodec.startsWith("libvpx") || vcodec.startsWith("libaom-")) {
                params.append(QStringLiteral("crf=%quality"));
                params.append(QStringLiteral("vb=0")); // VP9 needs this to prevent constrained quality mode.
            } else if (vcodec.endsWith("_vaapi")) {
                params.append(QStringLiteral("vglobal_quality=%quality"));
                params.append(QStringLiteral("vq=%quality"));
            } else {
                params.append(QStringLiteral("qscale=%quality"));
            }
            break;
        }
        case RenderPresetParams::RateControl::Constrained: {
            if (vcodec.startsWith("libx264") || vcodec.startsWith("libvpx") || vcodec.startsWith("libaom-") || vcodec.startsWith("libsvtav1")) {
                params.append(QStringLiteral("crf=%quality"));
            } else if (vcodec.endsWith("_qsv") || vcodec.endsWith("_videotoolbox")) {
                params.append(QStringLiteral("vb=%cvbr")); // setIfNotSet(p, "vb", qRound(cvbr));
            } else if (vcodec.endsWith("_vaapi")) {
                params.append(QStringLiteral("vb=%bitrate+'k'"));
                params.append(QStringLiteral("vglobal_quality=%quality"));
                params.append(QStringLiteral("vq=%quality"));
            } else {
                params.append(QStringLiteral("qscale=%quality"));
            }
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
        }
        }
        if (fixedGop->isEnabled() && fixedGop->isChecked()) {
            if (vcodec.startsWith("libvpx") || vcodec.startsWith("libaom-")) {
                params.append(QStringLiteral("keyint_min=%1").arg(gopSpinner->value()));
            } else {
                params.append(QStringLiteral("sc_threshold=0"));
            }
        }
        /*
        if (vcodec.endsWith("_videotoolbox")) {
            setIfNotSet(p, "pix_fmt", "nv12");
        } else if (vcodec.endsWith("_vaapi")) {
            setIfNotSet(p, "vprofile", "main");
            setIfNotSet(p, "connection_type", "x11");
        }*/
    }
    if (gopSpinner->value() > 0) {
        params.append(QStringLiteral("g=%1").arg(gopSpinner->value()));
        if (bFramesSpinner->value() >= 0) {
            params.append(QStringLiteral("bf=%1").arg(bFramesSpinner->value()));
        }
    }

    // audio tab
    QString acodec = aCodecCombo->currentText();
    params.append(QStringLiteral("acodec=%1").arg(acodec));

    if (cChannels->isChecked() && audioChannels->currentData().toInt() > 0) {
        params.append(QStringLiteral("channels=%1").arg(audioChannels->currentData().toInt()));
    }
    if (cSampleR->isChecked() && audioSampleRate->currentText().toInt() > 0) {
        params.append(QStringLiteral("ar=%1").arg(audioSampleRate->currentText().toInt()));
    }
    if (acodec.startsWith(QStringLiteral("pcm_"))) {
        aRateControlCombo->setEnabled(false);
        aBitrate->setEnabled(false);
        aQuality->setEnabled(false);
    } else {
        aRateControlCombo->setEnabled(true);
        switch (aRateControlCombo->currentIndex()) {
        case RenderPresetParams::RateControl::Unknown:
            aBitrate->setEnabled(false);
            aQuality->setEnabled(false);
            break;
        case RenderPresetParams::RateControl::Average:
        case RenderPresetParams::RateControl::Constant:
            aBitrate->setEnabled(true);
            aQuality->setEnabled(false);
            break;
        case RenderPresetParams::RateControl::Quality:
        default:
            aBitrate->setEnabled(false);
            aQuality->setEnabled(true);
            break;
        };
        switch (aRateControlCombo->currentIndex()) {
        case RenderPresetParams::RateControl::Average:
        case RenderPresetParams::RateControl::Constant: {
            params.append(QStringLiteral("ab=%audiobitrate+'k'"));
            if (acodec == "libopus") {
                if (aRateControlCombo->currentIndex() == RenderPresetParams::RateControl::Constant) {
                    params.append(QStringLiteral("vbr=off"));
                } else {
                    params.append(QStringLiteral("vbr=constrained"));
                }
            }
            break;
        }
        case RenderPresetParams::RateControl::Quality: {
            if (acodec == "libopus") {
                params.append(QStringLiteral("vbr=on"));
                params.append(QStringLiteral("compression_level=%audioquality"));
            } else {
                params.append(QStringLiteral("aq=%audioquality"));
            }
        }
        default:
            break;
        }
    }

    QString addionalParams = additionalParams->toPlainText().simplified();

    QStringList removed;
    for (const auto &p : std::as_const(m_uiParams)) {
        QString store = addionalParams;
        if (store != addionalParams.remove(QRegularExpression(QStringLiteral("((^|\\s)%1=\\S*)").arg(p)))) {
            removed.append(p);
        }
    }
    if (!removed.isEmpty()) {
        overrideParamsWarning->setText(
            xi18nc("@info",
                   "The following parameters will not have an effect, because they get overwritten by the equivalent user interface options: <icode>%1</icode>",
                   removed.join(", ")));
        overrideParamsWarning->show();
    } else {
        overrideParamsWarning->hide();
    }
    addionalParams = addionalParams.simplified().prepend(params.join(QStringLiteral(" ")) + QStringLiteral(" "));
    parameters->setPlainText(addionalParams.simplified());
}

void RenderPresetDialog::setPixelAspectRatio(int num, int den)
{
    parCombo->blockSignals(true);
    if (num < 1) num = 1;
    if (den < 1) den = 1;
    int gcdV = gcd(num, den);
    QString data = QStringLiteral("%1:%2").arg(num / gcdV).arg(den / gcdV);
    int ix = parCombo->findData(data);
    if (ix < 0) {
        parCombo->addItem(QStringLiteral("%L1 (%2:%3)").arg(double(num) / double(den), 0, 'g', 8).arg(num / gcdV).arg(den / gcdV), data);
        ix = parCombo->count() - 1;
    }
    parCombo->setCurrentIndex(ix);
    parCombo->blockSignals(false);
}

void RenderPresetDialog::updateDisplayAspectRatio()
{
    displayAspectNum->blockSignals(true);
    displayAspectDen->blockSignals(true);
    QStringList par = parCombo->currentData().toString().split(QStringLiteral(":"));
    int parNum = resWidth->value();
    int parDen = resHeight->value();
    if (par.length() >= 2 && par.at(0).toInt() > 0 && par.at(1).toInt() > 0) {
        parNum *= par.at(0).toInt();
        parDen *= par.at(1).toInt();
    }
    int gcdV = gcd(parNum, parDen);
    displayAspectNum->setValue(parNum / gcdV);
    displayAspectDen->setValue(parDen / gcdV);
    displayAspectNum->blockSignals(false);
    displayAspectDen->blockSignals(false);
    slotUpdateParams();
};

QString RenderPresetDialog::saveName()
{
    return m_saveName;
}
