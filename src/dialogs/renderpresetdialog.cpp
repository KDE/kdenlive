/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderpresetdialog.h"

#include "profiles/profilemodel.hpp"
#include "renderpresets/renderpresetmodel.hpp"
#include "renderpresets/renderpresetrepository.hpp"
#include "monitor/monitormanager.h"
#include "core.h"

#include <KMessageBox>
#include <QPushButton>

RenderPresetDialog::RenderPresetDialog(QWidget *parent, RenderPresetModel *preset, Mode mode)
    : QDialog(parent)
    , m_saveName()
    , m_monitor(nullptr)
{
    setupUi(this);

    m_uiParams.append({
                          QStringLiteral("f"),
                          QStringLiteral("acodec"),
                          QStringLiteral("vcodec"),
                          QStringLiteral("width"),
                          QStringLiteral("height"),
                          QStringLiteral("s"),
                          QStringLiteral("r"),
                          QStringLiteral("frame_rate_num"),
                          QStringLiteral("frame_rate_den"),
                          QStringLiteral("crf"),
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
                          QStringLiteral("channels")
                      });

    formatCombo->addItems(RenderPresetRepository::supportedFormats());
    vCodecCombo->addItems(RenderPresetRepository::vcodecs());
    aCodecCombo->addItems(RenderPresetRepository::acodecs());

    aRateControlCombo->addItem(i18n("Average Bitrate"));
    aRateControlCombo->addItem(i18n("CBR – Constant Bitrate"));
    aRateControlCombo->addItem(i18n("VBR – Variable Bitrate"));

    audioChannels->addItem(i18n("1 (mono)"), 1);
    audioChannels->addItem(i18n("2 (stereo)"), 2);
    audioChannels->addItem(i18n("4"), 4);
    audioChannels->addItem(i18n("6"), 6);

    QValidator *validator = new QIntValidator(this);
    audioSampleRate->setValidator(validator);

    vRateControlCombo->addItem(i18n("Average Bitrate"));
    vRateControlCombo->addItem(i18n("CBR – Constant Bitrate"));
    vRateControlCombo->addItem(i18n("VBR – Variable Bitrate"));
    vRateControlCombo->addItem(i18n("Contrained VBR"));

    connect(vRateControlCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index) {
        switch (index) {
            case RenderPresetModel::RateControl::Average:
                default_vbitrate->setEnabled(true);
                default_vquality->setEnabled(false);
                vBuffer->setEnabled(false);
                break;
            case RenderPresetModel::RateControl::Constant:
                default_vbitrate->setEnabled(true);
                default_vquality->setEnabled(false);
                vBuffer->setEnabled(true);
                break;
            case RenderPresetModel::RateControl::Constrained:
                default_vbitrate->setEnabled(true);
                default_vquality->setEnabled(true);
                vBuffer->setEnabled(true);
                break;
            case RenderPresetModel::RateControl::Quality:
                default_vbitrate->setEnabled(false);
                default_vquality->setEnabled(true);
                vBuffer->setEnabled(false);
                break;
        };
        slotUpdateParams();
    });

    groupName->addItems(RenderPresetRepository::get()->groupNames());

    std::unique_ptr<ProfileModel> &projectProfile = pCore->getCurrentProfile();
    if (preset) {
        groupName->setCurrentText(preset->groupName());
        if (mode != Mode::New) {
            preset_name->setText(preset->name());
        }
        preset_name->setFocus();
        formatCombo->setCurrentText(preset->getParam(QStringLiteral("f")));
        QString width = preset->getParam(QStringLiteral("width"));
        QString height = preset->getParam(QStringLiteral("height"));
        QString size = preset->getParam(QStringLiteral("s"));

        if (!(width.isEmpty() && height.isEmpty())) {
            resWidth->setValue(width.toInt());
            resHeight->setValue(height.toInt());
        } else if (!size.isEmpty()){
            QStringList list = size.split(QStringLiteral("x"));
            if (list.count() == 2) {
                resWidth->setValue(list.at(0).toInt());
                resHeight->setValue(list.at(1).toInt());
            }
        } else {
            resWidth->setValue(projectProfile->width());
            resHeight->setValue(projectProfile->height());
        }

        // video tab
        QString vbufsize = preset->getParam(QStringLiteral("vbufsize"));
        QString vqParam = preset->getParam(QStringLiteral("vq"));
        QString vbParam = preset->getParam(QStringLiteral("vb"));
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

        vRateControlCombo->setCurrentIndex(preset->videoRateControl());

        if (vbParam.isEmpty()) {
            vbParam = preset->getParam(QStringLiteral("vmaxrate"));
        }
        if (vbParam.contains(QStringLiteral("%bitrate"))) {
            default_vbitrate->setValue(preset->defaultVBitrate().toInt());
        } else {
            default_vbitrate->setValue(vbParam.replace('k', "").replace('M', "000").toInt());
        }

        QString sampAspNum = preset->getParam(QStringLiteral("sample_aspect_num"));
        QString sampAspDen = preset->getParam(QStringLiteral("sample_aspect_den"));
        QString sampAsp = preset->getParam(QStringLiteral("aspect"));

        if (!(sampAspNum.isEmpty() && sampAspDen.isEmpty())) {
            pixelAspectNum->setValue(sampAspNum.toInt());
            pixelAspectDen->setValue(sampAspDen.toInt());
        } else if (!sampAsp.isEmpty()) {
            QStringList list = sampAsp.split(QStringLiteral("/"));
            if (list.count() == 2) {
                pixelAspectNum->setValue(list.at(0).toInt());
                pixelAspectDen->setValue(list.at(1).toInt());
            }
        } else {
            pixelAspectNum->setValue(projectProfile->sample_aspect_num());
            pixelAspectDen->setValue(projectProfile->sample_aspect_den());
        }

        if (preset->hasParam(QStringLiteral("display_aspect_num"))
                && preset->hasParam(QStringLiteral("display_aspect_den"))) {
            displayAspectNum->setValue(preset->getParam(QStringLiteral("display_aspect_num")).toInt());
            displayAspectDen->setValue(preset->getParam(QStringLiteral("display_aspect_den")).toInt());
        } else {
            displayAspectNum->setValue(projectProfile->display_aspect_num());
            displayAspectDen->setValue(projectProfile->display_aspect_den());
        }

        vCodecCombo->setCurrentText(preset->getParam(QStringLiteral("vcodec")));
        if (!preset->getParam(QStringLiteral("r")).isEmpty()) {
            double val = preset->getParam(QStringLiteral("r")).toDouble();
            if (val == 23.98 || val == 23.976 || val == (24000/1001)) {
                framerateNum->setValue(24000);
                framerateDen->setValue(1001);
            } else if (val == 29.97 || val == (30000/1001)) {
                framerateNum->setValue(30000);
                framerateDen->setValue(1001);
            } else if (val == 47.95 || val == (48000/1001)) {
                framerateNum->setValue(48000);
                framerateDen->setValue(1001);
            } else if (val == 59.94 || val == (60000/1001)) {
                framerateNum->setValue(60000);
                framerateDen->setValue(1001);
            } else {
                framerateNum->setValue(qRound(val * 1000));
                framerateDen->setValue(1000);
            }
        } else if (preset->hasParam(QStringLiteral("frame_rate_num"))
                   && preset->hasParam(QStringLiteral("frame_rate_den"))) {
            framerateNum->setValue(preset->getParam(QStringLiteral("frame_rate_num")).toInt());
            framerateDen->setValue(preset->getParam(QStringLiteral("frame_rate_den")).toInt());
        } else {
            framerateNum->setValue(projectProfile->frame_rate_num());
            framerateDen->setValue(projectProfile->frame_rate_den());
        }
        scanningCombo->setCurrentIndex(preset->getParam(QStringLiteral("progressive")).toInt());
        fieldOrderCombo->setCurrentIndex(preset->getParam(QStringLiteral("top_field_first")).toInt());
        int gopVal = preset->getParam(QStringLiteral("g")).toInt();
        gopSpinner->setValue(gopVal);
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
        int ix = audioChannels->findData(preset->getParam(QStringLiteral("channels")).toInt());
        audioChannels->setCurrentIndex(ix);
        aRateControlCombo->setCurrentIndex(preset->audioRateControl());
        audioSampleRate->setCurrentText(preset->getParam(QStringLiteral("ar")));

        QString aqParam = preset->getParam(QStringLiteral("aq"));
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
        additionalParams->setPlainText(preset->params(m_uiParams));
    } else {
        resHeight->setValue(projectProfile->height());
        resWidth->setValue(projectProfile->width());
        framerateNum->setValue(projectProfile->frame_rate_num());
        framerateDen->setValue(projectProfile->frame_rate_den());
        pixelAspectNum->setValue(projectProfile->sample_aspect_num());
        pixelAspectDen->setValue(projectProfile->sample_aspect_den());
        displayAspectNum->setValue(projectProfile->display_aspect_num());
        displayAspectDen->setValue(projectProfile->display_aspect_den());
        scanningCombo->setCurrentIndex(projectProfile->progressive() ? 1 : 0);
    }

    if (groupName->currentText().isEmpty()) {
        groupName->setCurrentText(i18nc("Group Name", "Custom"));
    }

    if (mode == Mode::Edit) {
        setWindowTitle(i18n("Edit Render Preset"));
    }

    connect(preset_name, &QLineEdit::textChanged, this, [&](const QString &text){
            buttonBox->button(QDialogButtonBox::Ok)->setDisabled(text.isEmpty());
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, [&, preset, mode](){
        QString oldName;
        if (preset) {
            oldName = preset->name();
        }
        QString newPresetName = preset_name->text().simplified();
        if (newPresetName.isEmpty()) {
            KMessageBox::warningContinueCancel(this, i18n("The preset name can't be empty"));
            return;
        }
        QString newGroupName = groupName->currentText().simplified();
        if (newGroupName.isEmpty()) {
            newGroupName = i18nc("Group Name", "Custom");
        }
        QString speeds_list_str = speeds_list->toPlainText().replace('\n', ';').simplified();

        std::unique_ptr<RenderPresetModel> newPreset(new RenderPresetModel(newPresetName,
                                                                           newGroupName,
                                                                           parameters->toPlainText().simplified(),
                                                                           QString::number(default_vbitrate->value()),
                                                                           QString::number(default_vquality->value()),
                                                                           QString::number(aBitrate->value()),
                                                                           QString::number(aQuality->value()),
                                                                           speeds_list_str));

        m_saveName = RenderPresetRepository::get()->savePreset(newPreset.get());
        if ((mode == Mode::Edit) && !m_saveName.isEmpty() && (oldName != m_saveName)) {
            RenderPresetRepository::get()->deletePreset(oldName);
        }
        accept();
    });

    connect(formatCombo, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(vCodecCombo, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(scanningCombo, &QComboBox::currentTextChanged, this, [&](){
        fieldOrderCombo->setEnabled(scanningCombo->currentIndex() != 1);
        slotUpdateParams();
    });
    connect(fieldOrderCombo, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(aCodecCombo, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(resWidth, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(resHeight, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(pixelAspectNum, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value){
        if (value == 0) {
            pixelAspectDen->blockSignals(true);
            pixelAspectDen->setValue(0);
            pixelAspectDen->blockSignals(false);
        }
        slotUpdateParams();
    });
    connect(pixelAspectDen, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value){
        if (value == 0) {
            pixelAspectNum->blockSignals(true);
            pixelAspectNum->setValue(0);
            pixelAspectNum->blockSignals(false);
        }
        slotUpdateParams();
    });
    connect(displayAspectNum, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(displayAspectDen, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(framerateNum, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value){
        if (value == 0) {
            framerateDen->blockSignals(true);
            framerateDen->setValue(0);
            framerateDen->blockSignals(false);
        }
        slotUpdateParams();
    });
    connect(framerateDen, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value){
        if (value == 0) {
            framerateNum->blockSignals(true);
            framerateNum->setValue(0);
            framerateNum->blockSignals(false);
        }
        slotUpdateParams();
    });
    connect(fixedGop, &QCheckBox::stateChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(gopSpinner, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(bFramesSpinner, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(additionalParams, &QPlainTextEdit::textChanged, this, &RenderPresetDialog::slotUpdateParams);

    connect(aRateControlCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(aBitrate, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(aQuality, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderPresetDialog::slotUpdateParams);
    connect(audioChannels, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);
    connect(audioSampleRate, &QComboBox::currentTextChanged, this, &RenderPresetDialog::slotUpdateParams);

    slotUpdateParams();
    // TODO
    if (false && m_monitor == nullptr) {
        //QString profile = DvdWizardVob::getDvdProfile(format);
        m_monitor = new Monitor(Kdenlive::RenderMonitor, pCore->monitorManager(), this);
        m_monitor->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        mainBox->addWidget(m_monitor);
        pCore->monitorManager()->appendMonitor(m_monitor);
        //m_monitor->setCustomProfile(profile, m_tc);
        pCore->monitorManager()->activateMonitor(Kdenlive::RenderMonitor);
        m_monitor->start();
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

void RenderPresetDialog::slotUpdateParams() {
    QStringList params;

    QString vcodec = vCodecCombo->currentText();
    params.append(QStringLiteral("f=%1").arg(formatCombo->currentText()));
    // video tab
    params.append(QStringLiteral("vcodec=%1").arg(vcodec));
    if (resWidth->value() > 0) {
        params.append(QStringLiteral("width=%1").arg(resWidth->value()));
    }
    if (resHeight->value() > 0) {
        params.append(QStringLiteral("height=%1").arg(resHeight->value()));
    }
    if (pixelAspectNum->value() > 0 && pixelAspectDen->value() > 0) {
        params.append(QStringLiteral("sample_aspect_num=%1 sample_aspect_den=%2")
                      .arg(pixelAspectNum->value())
                      .arg(pixelAspectDen->value()));
    }
    if (displayAspectNum->value() > 0 && displayAspectDen->value() > 0 ) {
        params.append(QStringLiteral("display_aspect_num=%1 display_aspect_den=%2")
                      .arg(displayAspectNum->value())
                      .arg(displayAspectDen->value()));
    }

    if (framerateNum->value() > 0 && framerateDen->value() > 0) {
        params.append(QStringLiteral("frame_rate_num=%1").arg(framerateNum->value()));
        params.append(QStringLiteral("frame_rate_den=%1").arg(framerateDen->value()));
        frameRateDisplay->setText(QString::number(double(framerateNum->value()) / double(framerateDen->value()), 'g', 10));
        frameRateDisplay->show();
    } else {
        frameRateDisplay->hide();
    }
    // Adjust scanning
    params.append(QStringLiteral("progressive=%1").arg(scanningCombo->currentIndex()));
    if (scanningCombo->currentIndex() == 0) {
        params.append(QStringLiteral("top_field_first=%1").arg(fieldOrderCombo->currentIndex()));
    }
    if (vcodec == "libx265") {
        // Most x265 parameters must be supplied through x265-params.
        switch (vRateControlCombo->currentIndex()) {
        case RenderPresetModel::RateControl::Average:
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            break;
        case RenderPresetModel::RateControl::Constant: {
            // x265 does not expect bitrate suffixes and requires Kb/s
            params.append(QStringLiteral("vb=%bitrate"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
            }
        case RenderPresetModel::RateControl::Quality: {
            params.append(QStringLiteral("crf=%quality"));
            break;
            }
        case RenderPresetModel::RateControl::Constrained: {
            params.append(QStringLiteral("crf=%quality"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            break;
            }
        }
        if (fixedGop->isChecked()) {
            params.append(QStringLiteral("sc_threshold=0"));
        }
    } else if (vcodec.contains("nvenc")) {
        switch (vRateControlCombo->currentIndex()) {
        case RenderPresetModel::RateControl::Average:
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            break;
        case RenderPresetModel::RateControl::Constant: {
            params.append(QStringLiteral("cbr=1 "));
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            params.append(QStringLiteral("vminrate=%bitrate+'k'"));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
            }
        case RenderPresetModel::RateControl::Quality: {
            params.append(QStringLiteral("rc=constqp"));
            params.append(QStringLiteral("vglobal_quality=%quality"));
            params.append(QStringLiteral("vq=%quality"));
            break;
            }
        case RenderPresetModel::RateControl::Constrained: {
            params.append(QStringLiteral("qmin=%quality"));
            params.append(QStringLiteral("vb=%cvbr")); //setIfNotSet(p, "vb", qRound(cvbr));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
            }
        }
        /*if (ui->dualPassCheckbox->isChecked())
            setIfNotSet(p, "v2pass", 1);
        */
        if (fixedGop->isChecked()) {
            params.append(QStringLiteral("sc_threshold=0 strict_gop=1"));
        }
    } else if (vcodec.endsWith("_amf")) {
        switch (vRateControlCombo->currentIndex()) {
        case RenderPresetModel::RateControl::Average:
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            break;
        case RenderPresetModel::RateControl::Constant: {
            params.append(QStringLiteral("rc=cbr "));
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            params.append(QStringLiteral("vminrate=%bitrate+'k'"));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1 ").arg(vBuffer->value() * 8 * 1024));
            break;
            }
        case RenderPresetModel::RateControl::Quality: {
            params.append(QStringLiteral("rc=cqp"));
            params.append(QStringLiteral("qp_i=%quality"));
            params.append(QStringLiteral("qp_p=%quality"));
            params.append(QStringLiteral("qp_b=%quality"));
            params.append(QStringLiteral("vq=%quality"));
            break;
            }
        case RenderPresetModel::RateControl::Constrained: {
            params.append(QStringLiteral("rc=vbr_peak"));
            params.append(QStringLiteral("qmin=%quality"));
            params.append(QStringLiteral("vb=%cvbr")); //setIfNotSet(p, "vb", qRound(cvbr));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
            }
        }
        /*if (ui->dualPassCheckbox->isChecked())
            setIfNotSet(p, "v2pass", 1);
            */
        if (fixedGop->isChecked()) {
            params.append(QStringLiteral("sc_threshold=0 strict_gop=1"));
        }
    } else {
        switch (vRateControlCombo->currentIndex()) {
        case RenderPresetModel::RateControl::Average:
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            break;
        case RenderPresetModel::RateControl::Constant: {
            //const QString& b = ui->videoBitrateCombo->currentText();
            params.append(QStringLiteral("vb=%bitrate+'k'"));
            params.append(QStringLiteral("vminrate=%bitrate+'k'"));
            params.append(QStringLiteral("vmaxrate=%bitrate+'k'"));
            params.append(QStringLiteral("vbufsize=%1").arg(vBuffer->value() * 8 * 1024));
            break;
            }
        case RenderPresetModel::RateControl::Quality: {
            if (vcodec.startsWith("libx264")) {
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
        case RenderPresetModel::RateControl::Constrained: {
            if (vcodec.startsWith("libx264") || vcodec.startsWith("libvpx") || vcodec.startsWith("libaom-")) {
                params.append(QStringLiteral("crf=%quality"));
            } else if (vcodec.endsWith("_qsv") || vcodec.endsWith("_videotoolbox")) {
                params.append(QStringLiteral("vb=%cvbr")); //setIfNotSet(p, "vb", qRound(cvbr));
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
        if (fixedGop->isChecked()) {
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
    params.append(QStringLiteral("g=%1").arg(gopSpinner->value()));
    params.append(QStringLiteral("bf=%1").arg(bFramesSpinner->value()));

    // audio tab
    QString acodec = aCodecCombo->currentText();
    params.append(QStringLiteral("acodec=%1").arg(acodec));

    if (audioChannels->currentData().toInt() > 0) {
        params.append(QStringLiteral("channels=%1").arg(audioChannels->currentData().toInt()));
    }
    if (audioSampleRate->currentText().toInt() > 0) {
        params.append(QStringLiteral("ar=%1").arg(audioSampleRate->currentText().toInt()));
    }
    if (acodec.startsWith(QStringLiteral("pcm_"))) {
        aRateControlCombo->setEnabled(false);
        aBitrate->setEnabled(false);
        aQuality->setEnabled(false);
    } else {
        aRateControlCombo->setEnabled(true);
        switch (aRateControlCombo->currentIndex()) {
            case RenderPresetModel::RateControl::Average:
            case RenderPresetModel::RateControl::Constant:
                aBitrate->setEnabled(true);
                aQuality->setEnabled(false);
                break;
            case RenderPresetModel::RateControl::Quality:
            default:
                aBitrate->setEnabled(false);
                aQuality->setEnabled(true);
                break;
        };
        if (aRateControlCombo->currentIndex() == RenderPresetModel::RateControl::Average
                || aRateControlCombo->currentIndex() == RenderPresetModel::RateControl::Constant) {
            params.append(QStringLiteral("ab=%audiobitrate+'k'"));
            if (acodec == "libopus") {
                if (aRateControlCombo->currentIndex() == RenderPresetModel::RateControl::Constant) {
                    params.append(QStringLiteral("vbr=off"));
                } else {
                    params.append(QStringLiteral("vbr=constrained"));
                }
            }
        } else if (acodec == "libopus") {
            params.append(QStringLiteral("vbr=on"));
            // TODO
            //params.append(QStringLiteral("compression_level= "));
            //setIfNotSet(p, "compression_level", TO_ABSOLUTE(0, 10, ui->audioQualitySpinner->value()));
        } else {
            params.append(QStringLiteral("aq=%audioquality"));
        }
    }

    QString addionalParams = additionalParams->toPlainText().simplified();

    QStringList removed;
    for (auto p : m_uiParams) {
        QString store = addionalParams;
        if (store != addionalParams.remove(QRegularExpression(QStringLiteral("((^|\\s)%1=\\S*)").arg(p)))) {
              removed.append(p);
        }
    }
    if (!removed.isEmpty()) {
        overrideParamsWarning->setText(xi18nc("@info", "The following parameters will not have an effect, because they get overwritten by the equivalent user interface options: <icode>%1</icode>", removed.join(", ")));
        overrideParamsWarning->show();
    } else {
        overrideParamsWarning->hide();
    }
    addionalParams = addionalParams.simplified().prepend(params.join(QStringLiteral(" ")) + QStringLiteral(" "));

    parameters->setText(addionalParams.simplified());
}

QString RenderPresetDialog::saveName()
{
    return m_saveName;
}
