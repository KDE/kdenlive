/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "titletemplatedialog.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"

#include "klocalizedstring.h"
#include <KComboBox>
#include <QDir>
#include <QStandardPaths>

TitleTemplateDialog::TitleTemplateDialog(const QString &folder, QWidget *parent)
    : QDialog(parent)
{
    m_view.setupUi(this);
    // Get the list of existing templates
    const QStringList filter = {QStringLiteral("*.kdenlivetitle")};
    const QString path = folder + QStringLiteral("/titles/");

    // Project templates
    QDir dir(path);
    const QStringList templateFiles = dir.entryList(filter, QDir::Files);
    for (const QString &fname : templateFiles) {
        m_view.template_list->comboBox()->addItem(fname, dir.absoluteFilePath(fname));
    }

    // System templates
    QStringList titleTemplates = QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("titles/"), QStandardPaths::LocateDirectory);
    titleTemplates.removeDuplicates();
    for (const QString &folderpath : std::as_const(titleTemplates)) {
        QDir sysdir(folderpath);
        const QStringList filenames = sysdir.entryList(filter, QDir::Files);
        for (const QString &fname : filenames) {
            m_view.template_list->comboBox()->addItem(fname, sysdir.absoluteFilePath(fname));
        }
    }

    if (m_view.template_list->comboBox()->count() > 0) {
        m_view.buttonBox->button(QDialogButtonBox::Ok)->setFocus();
    }
    int current = m_view.template_list->comboBox()->findText(KdenliveSettings::selected_template());
    if (current > -1) {
        m_view.template_list->comboBox()->setCurrentIndex(current);
    }
    const QStringList mimeTypeFilters = {QStringLiteral("application/x-kdenlivetitle")};
    m_view.template_list->setMimeTypeFilters(mimeTypeFilters);
    connect(m_view.template_list->comboBox(), static_cast<void (KComboBox::*)(int)>(&KComboBox::currentIndexChanged), this,
            &TitleTemplateDialog::updatePreview);
    connect(m_view.description, &QTextEdit::textChanged, this, &TitleTemplateDialog::updatePreview);
}

QString TitleTemplateDialog::selectedTemplate() const
{
    QString textTemplate = m_view.template_list->comboBox()->itemData(m_view.template_list->comboBox()->currentIndex()).toString();
    if (textTemplate.isEmpty()) {
        textTemplate = m_view.template_list->comboBox()->currentText();
    }
    return textTemplate;
}

QString TitleTemplateDialog::selectedText() const
{
    return m_view.description->toPlainText();
}

void TitleTemplateDialog::resizeEvent(QResizeEvent *event)
{
    updatePreview();
    QWidget::resizeEvent(event);
}

void TitleTemplateDialog::updatePreview()
{
    QString textTemplate = m_view.template_list->comboBox()->itemData(m_view.template_list->comboBox()->currentIndex()).toString();
    if (textTemplate.isEmpty()) {
        textTemplate = m_view.template_list->comboBox()->currentText();
    }
    QMap<QString, QString> producerParams;
    producerParams.insert(QStringLiteral("templatetext"), m_view.description->toPlainText());
    QUrl titleUrl = QUrl::fromLocalFile(textTemplate);
    QPixmap pix = KThumb::getImageWithParams(titleUrl, producerParams, m_view.preview->width());
    m_view.preview->setPixmap(pix);
    KdenliveSettings::setSelected_template(m_view.template_list->comboBox()->currentText());
    if (brokenUrls.contains(textTemplate)) {
        m_view.errorMessage->setText(i18n("Template %1 does not contain the <b>%s</b> keyword required to replace the text.", titleUrl.fileName()));
        m_view.errorMessage->animatedShow();
    } else if (validatedUrls.contains(textTemplate)) {
        m_view.errorMessage->hide();
    } else {
        // Validate url
        QFile f(textTemplate);
        if (!f.open(QFile::ReadOnly | QFile::Text)) {
            m_view.errorMessage->setText(i18n("Cannot read file %1.", titleUrl.fileName()));
            m_view.errorMessage->animatedShow();
            return;
        }
        QTextStream in(&f);
        if (f.size() < qint64(120000)) {
            const QString titleData = in.readAll();
            if (titleData.contains(QStringLiteral("%s"))) {
                validatedUrls << textTemplate;
                m_view.errorMessage->hide();
            } else {
                brokenUrls << textTemplate;
                m_view.errorMessage->setText(i18n("Template %1 does not contain the <b>%s</b> keyword required to replace the text.", titleUrl.fileName()));
                m_view.errorMessage->animatedShow();
            }
        }
    }
}
