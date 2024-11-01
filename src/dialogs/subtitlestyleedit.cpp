/*  This file is part of Kdenlive. See www.kdenlive.org.
    SPDX-FileCopyrightText: 2024 Chengkun Chen <serix2004@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "subtitlestyleedit.h"
#include "bin/model/subtitlemodel.hpp"
#include "definitions.h"
#include "doc/kthumb.h"
#include <QMessageBox>
#include <mlt++/MltFilter.h>
#include <mlt++/MltProfile.h>
#include <qcolordialog.h>
#include <qfontdialog.h>

SubtitleStyleEdit::SubtitleStyleEdit(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(buttonSelectFont, &QPushButton::clicked, this, [this]() {
        QFont oldFont;
        oldFont.setFamily(labelFontName->text());
        oldFont.setPointSizeF(spinFontSize->text().toDouble());
        oldFont.setBold(checkBold->isChecked());
        oldFont.setItalic(checkItalic->isChecked());
        oldFont.setUnderline(checkUnderline->isChecked());
        oldFont.setStrikeOut(checkStrikeOut->isChecked());

        bool ok;
        QFont newFont = QFontDialog::getFont(&ok, oldFont, this, tr("Select font"));
        if (ok) {
            labelFontName->setText(newFont.family());
            spinFontSize->setValue(newFont.pointSizeF());
            checkBold->setChecked(newFont.bold());
            checkItalic->setChecked(newFont.italic());
            checkUnderline->setChecked(newFont.underline());
            checkStrikeOut->setChecked(newFont.strikeOut());
        }

        updateProperties();
    });
    buttonSelectFont->setToolTip(i18n("Select font"));
    buttonSelectFont->setWhatsThis(xi18nc("@info:whatsthis", "Open a font dialog to select a font and set its properties."));

    connect(spinFontSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(checkBold, &QCheckBox::stateChanged, this, &SubtitleStyleEdit::updateProperties);
    connect(checkItalic, &QCheckBox::stateChanged, this, &SubtitleStyleEdit::updateProperties);
    connect(checkUnderline, &QCheckBox::stateChanged, this, &SubtitleStyleEdit::updateProperties);
    connect(checkStrikeOut, &QCheckBox::stateChanged, this, &SubtitleStyleEdit::updateProperties);

    connect(buttonPrimaryColor, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(m_style.primaryColour(), this, tr("Select primary color"), QColorDialog::ShowAlphaChannel);
        if (color.isValid()) {
            buttonPrimaryColor->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name(QColor::HexArgb)));
            updateProperties();
        }
    });

    connect(buttonSecondaryColor, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(m_style.secondaryColour(), this, tr("Select secondary color"), QColorDialog::ShowAlphaChannel);
        if (color.isValid()) {
            buttonSecondaryColor->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name(QColor::HexArgb)));
            updateProperties();
        }
    });

    connect(buttonOutlineColor, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(m_style.outlineColour(), this, tr("Select outline color"), QColorDialog::ShowAlphaChannel);
        if (color.isValid()) {
            buttonOutlineColor->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name(QColor::HexArgb)));
            updateProperties();
        }
    });

    connect(buttonBackColor, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(m_style.backColour(), this, tr("Select background color"), QColorDialog::ShowAlphaChannel);
        if (color.isValid()) {
            buttonBackColor->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name(QColor::HexArgb)));
            updateProperties();
        }
    });

    connect(spinMarginL, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(spinMarginR, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(spinMarginV, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);

    connect(spinScaleX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(spinScaleY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(spinSpacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(spinAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(spinOutline, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(spinShadow, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleStyleEdit::updateProperties);

    comboBorderStyle->addItem(i18n("Default"), 1);
    comboBorderStyle->addItem(i18n("Box as Outline"), 3);
    comboBorderStyle->addItem(i18n("Box as Shadow"), 4);

    connect(comboBorderStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SubtitleStyleEdit::updateProperties);

    comboAlignment->addItem(i18n("Top Left"), 7);
    comboAlignment->addItem(i18n("Top"), 8);
    comboAlignment->addItem(i18n("Top Right"), 9);
    comboAlignment->addItem(i18n("Left"), 4);
    comboAlignment->addItem(i18n("Center"), 5);
    comboAlignment->addItem(i18n("Right"), 6);
    comboAlignment->addItem(i18n("Bottom Left"), 1);
    comboAlignment->addItem(i18n("Bottom"), 2);
    comboAlignment->addItem(i18n("Bottom Right"), 3);

    connect(comboAlignment, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SubtitleStyleEdit::updateProperties);
    connect(editPreview, &QLineEdit::textChanged, this, &SubtitleStyleEdit::updateProperties);

    labelPreview->hide();
    preview->hide();
    editPreview->hide();
    horizontalSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMaximumSize(QSize(0, 0));

    connect(buttonPreview, &QPushButton::clicked, this, [this]() {
        if (labelPreview->isVisible()) {
            labelPreview->hide();
            preview->hide();
            editPreview->hide();
            buttonPreview->setText(i18n("Show Preview >>>"));
            horizontalSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        } else {
            labelPreview->show();
            preview->show();
            editPreview->show();
            buttonPreview->setText(i18n("<<< Hide Preview"));
            horizontalSpacer->changeSize(20, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        }
    });
}

SubtitleStyleEdit::~SubtitleStyleEdit() {}

const SubtitleStyle SubtitleStyleEdit::getStyle(QWidget *parent, const SubtitleStyle &style, QString &styleName, std::shared_ptr<const SubtitleModel> model,
                                                bool global, bool *ok)
{
    SubtitleStyleEdit *dialog = new SubtitleStyleEdit(parent);
    QStringList existingStyleNames;
    for (auto &style : model->getAllSubtitleStyles(global)) {
        existingStyleNames.append(style.first);
    }
    const int previewWidth = model->scriptInfo().at("PlayResX").toInt();
    const int previewHeight = model->scriptInfo().at("PlayResY").toInt();
    const QString previewPath = model->previewSubtitlePath();
    dialog->setProperties(style, styleName, previewWidth, previewHeight, previewPath, ok);
    if (!global && styleName == "Default") {
        dialog->editName->setEnabled(false);
    }
    while (true) {
        if (dialog->exec() == QDialog::Accepted) {
            // check if style name is unique
            if (existingStyleNames.end() != std::find(existingStyleNames.begin(), existingStyleNames.end(), dialog->editName->text()) &&
                dialog->editName->text() != styleName) {
                QMessageBox::warning(dialog, i18n("Error"), i18n("Style name already exists."));
                continue;
            }
            // check if style name is not empty
            if (dialog->editName->text().isEmpty()) {
                QMessageBox::warning(dialog, i18n("Error"), i18n("Style name cannot be empty."));
                continue;
            }
            // name must not have ","
            if (dialog->editName->text().contains(',')) {
                QMessageBox::warning(dialog, i18n("Error"), i18n("Style name cannot contain ','."));
                continue;
            }
            if (ok) *ok = true;
            styleName = dialog->editName->text();
            const SubtitleStyle newStyle = dialog->m_style;
            delete dialog;
            return newStyle;
        } else {
            if (ok) *ok = false;
            delete dialog;
            return style;
        }
    }
}

void SubtitleStyleEdit::setProperties(const SubtitleStyle &style, const QString &styleName, const int previewWidth, const int previewHeight,
                                      const QString previewPath, bool *ok)
{
    QSignalBlocker bk1(editName);
    QSignalBlocker bk2(spinFontSize);
    QSignalBlocker bk3(checkBold);
    QSignalBlocker bk4(checkItalic);
    QSignalBlocker bk5(checkUnderline);
    QSignalBlocker bk6(checkStrikeOut);
    QSignalBlocker bk7(spinMarginL);
    QSignalBlocker bk8(spinMarginR);
    QSignalBlocker bk9(spinMarginV);
    QSignalBlocker bk10(spinScaleX);
    QSignalBlocker bk11(spinScaleY);
    QSignalBlocker bk12(spinSpacing);
    QSignalBlocker bk13(spinAngle);
    QSignalBlocker bk14(spinOutline);
    QSignalBlocker bk15(spinShadow);
    QSignalBlocker bk16(comboBorderStyle);
    QSignalBlocker bk17(comboAlignment);
    QSignalBlocker bk18(editPreview);

    m_style = style;

    labelFontName->setText(style.fontName());
    spinFontSize->setValue(style.fontSize());
    checkBold->setChecked(style.bold());
    checkItalic->setChecked(style.italic());
    checkUnderline->setChecked(style.underline());
    checkStrikeOut->setChecked(style.strikeOut());

    buttonPrimaryColor->setStyleSheet(QStringLiteral("background-color: %1").arg(style.primaryColour().name(QColor::HexArgb)));
    buttonSecondaryColor->setStyleSheet(QStringLiteral("background-color: %1").arg(style.secondaryColour().name(QColor::HexArgb)));
    buttonOutlineColor->setStyleSheet(QStringLiteral("background-color: %1").arg(style.outlineColour().name(QColor::HexArgb)));
    buttonBackColor->setStyleSheet(QStringLiteral("background-color: %1").arg(style.backColour().name(QColor::HexArgb)));

    spinMarginL->setValue(style.marginL());
    spinMarginR->setValue(style.marginR());
    spinMarginV->setValue(style.marginV());

    spinScaleX->setValue(style.scaleX());
    spinScaleY->setValue(style.scaleY());
    spinSpacing->setValue(style.spacing());
    spinAngle->setValue(style.angle());
    spinOutline->setValue(style.outline());
    spinShadow->setValue(style.shadow());

    comboBorderStyle->setCurrentIndex(comboBorderStyle->findData(style.borderStyle()));
    comboAlignment->setCurrentIndex(comboAlignment->findData(style.alignment()));

    editPreview->setText("Add Text");
    editName->setText(styleName);
    m_ok = ok;

    if (!m_previewProducer) {
        m_width = previewWidth;
        m_height = previewHeight;
        m_previewPath = previewPath;

        // create a new subtitle file
        m_previewFile.setFileName(m_previewPath);
        m_previewFile.open(QIODevice::WriteOnly);
        QString fileContent = QStringLiteral("[Script Info]\n; Script generated by Kdenlive for preview purpose\nScriptType: v4.00+\n"
                                             "PlayResX: %1\nPlayResY: %2\nLayoutResX: %3\nLayoutResY: %4\nWrapStyle: 0\n"
                                             "ScaledBorderAndShadow: yes\nYCbCr Matrix: None\n\n"
                                             "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, "
                                             "Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, "
                                             "MarginR, MarginV, Encoding\n"
                                             "%5\n\n"
                                             "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
                                             "Dialogue: 0,00:00:00.00,00:00:01.00,%6,,0,0,0,,%7\n")
                                  .arg(m_width)
                                  .arg(m_height)
                                  .arg(m_width)
                                  .arg(m_height)
                                  .arg(style.toString("Default"))
                                  .arg("Default")
                                  .arg(editPreview->text());
        m_previewFile.write(fileContent.toUtf8());
        m_previewFile.close();
        // create a temp producer for preview
        // apply a higher resolution to avoid blurry text
        m_previewProfile = std::make_unique<Mlt::Profile>();
        m_previewProfile->set_height(m_width);
        m_previewProfile->set_width(m_height);
        m_previewProducer = std::make_unique<Mlt::Producer>(*m_previewProfile.get(), "");
        m_previewProducer->set("width", m_width);
        m_previewProducer->set("height", m_height);
        m_previewProducer->set("audio_index", -1);
        // attach av.subtitles filter
        m_previewFilter = std::make_unique<Mlt::Filter>(*m_previewProfile.get(), "avfilter.subtitles");
        m_previewFilter->connect(*m_previewProducer);
        m_previewFilter->set("av.filename", m_previewPath.toStdString().c_str());
        m_previewFilter->set("av.alpha", 1);

        Mlt::Frame *frame = m_previewFilter->get_frame();
        QImage p = KThumb::getFrame(frame, m_width, m_height);
        delete frame;
        preview->setPixmap(QPixmap::fromImage(p));
    }
}

void SubtitleStyleEdit::updateProperties()
{
    m_style.setFontName(labelFontName->text());
    m_style.setFontSize(spinFontSize->value());
    m_style.setBold(checkBold->isChecked());
    m_style.setItalic(checkItalic->isChecked());
    m_style.setUnderline(checkUnderline->isChecked());
    m_style.setStrikeOut(checkStrikeOut->isChecked());

    m_style.setPrimaryColour(QColor(buttonPrimaryColor->styleSheet().split(':').at(1).trimmed()));
    m_style.setSecondaryColour(QColor(buttonSecondaryColor->styleSheet().split(':').at(1).trimmed()));
    m_style.setOutlineColour(QColor(buttonOutlineColor->styleSheet().split(':').at(1).trimmed()));
    m_style.setBackColour(QColor(buttonBackColor->styleSheet().split(':').at(1).trimmed()));

    m_style.setMarginL(spinMarginL->value());
    m_style.setMarginR(spinMarginR->value());
    m_style.setMarginV(spinMarginV->value());

    m_style.setScaleX(spinScaleX->value());
    m_style.setScaleY(spinScaleY->value());
    m_style.setSpacing(spinSpacing->value());
    m_style.setAngle(spinAngle->value());
    m_style.setOutline(spinOutline->value());
    m_style.setShadow(spinShadow->value());

    m_style.setBorderStyle(comboBorderStyle->currentData().toInt());
    m_style.setAlignment(comboAlignment->currentData().toInt());

    m_previewFile.setFileName(m_previewPath);
    m_previewFile.open(QIODevice::WriteOnly);
    QString fileContent = QStringLiteral("[Script Info]\n; Script generated by Kdenlive for preview purpose\nScriptType: v4.00+\n"
                                         "PlayResX: %1\nPlayResY: %2\nLayoutResX: %3\nLayoutResY: %4\nWrapStyle: 0\n"
                                         "ScaledBorderAndShadow: yes\nYCbCr Matrix: None\n\n"
                                         "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, "
                                         "Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, "
                                         "MarginR, MarginV, Encoding\n"
                                         "%5\n\n"
                                         "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
                                         "Dialogue: 0,00:00:00.00,00:00:01.00,%6,,0,0,0,,%7\n")
                              .arg(m_width)
                              .arg(m_height)
                              .arg(m_width)
                              .arg(m_height)
                              .arg(m_style.toString("Default"))
                              .arg("Default")
                              .arg(editPreview->text());
    m_previewFile.write(fileContent.toUtf8());
    m_previewFile.close();

    m_previewFilter->set("av.filename", m_previewPath.toStdString().c_str());

    Mlt::Frame *frame = m_previewFilter->get_frame();
    QImage p = KThumb::getFrame(frame, m_width, m_height);
    delete frame;
    preview->setPixmap(QPixmap::fromImage(p));
}
