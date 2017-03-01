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

#include "titletemplatedialog.h"
#include "kdenlivesettings.h"
#include "utils/KoIconUtils.h"
#include "doc/kthumb.h"

#include "klocalizedstring.h"
#include <KComboBox>
#include <QDir>
#include <QStandardPaths>

TitleTemplateDialog::TitleTemplateDialog(const QString &folder, QWidget *parent) :
    QDialog(parent)
{
    m_view.setupUi(this);
    // Get the list of existing templates
    const QStringList filter = { QStringLiteral("*.kdenlivetitle") };
    const QString path = folder + QStringLiteral("/titles/");

    // Project templates
    QDir dir(path);
    const QStringList templateFiles = dir.entryList(filter, QDir::Files);
    for (const QString &fname : templateFiles) {
        m_view.template_list->comboBox()->addItem(fname, dir.absoluteFilePath(fname));
    }

    // System templates
    const QStringList titleTemplates = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("titles/"), QStandardPaths::LocateDirectory);

    for (const QString &folderpath : titleTemplates) {
        QDir folder(folderpath);
        const QStringList filesnames = folder.entryList(filter, QDir::Files);
        for (const QString &fname : filesnames) {
            m_view.template_list->comboBox()->addItem(fname, folder.absoluteFilePath(fname));
        }
    }

    if (!templateFiles.isEmpty()) {
        m_view.buttonBox->button(QDialogButtonBox::Ok)->setFocus();
    }
    const QStringList mimeTypeFilters = { QStringLiteral("application/x-kdenlivetitle") };
    m_view.template_list->setFilter(mimeTypeFilters.join(' '));
    connect(m_view.template_list->comboBox(), SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
    updatePreview();
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

void TitleTemplateDialog::updatePreview()
{
    QString textTemplate = m_view.template_list->comboBox()->itemData(m_view.template_list->comboBox()->currentIndex()).toString();
    if (textTemplate.isEmpty()) {
        textTemplate = m_view.template_list->comboBox()->currentText();
    }
    QPixmap pix = KThumb::getImage(QUrl::fromLocalFile(textTemplate), m_view.preview->width());
    m_view.preview->setPixmap(pix);
}
