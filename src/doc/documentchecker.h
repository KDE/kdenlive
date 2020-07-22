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

#ifndef DOCUMENTCHECKER_H
#define DOCUMENTCHECKER_H

#include "definitions.h"
#include "ui_missingclips_ui.h"

#include <QDir>
#include <QDomElement>
#include <QUrl>

class DocumentChecker : public QObject
{
    Q_OBJECT

public:
    explicit DocumentChecker(QUrl url, const QDomDocument &doc);
    ~DocumentChecker() override;
    /**
     * @brief checks for problems with the clips in the project
     * Checks for missing proxies, wrong duration clips, missing fonts, missing images, missing source clips
     * Calls DocumentChecker::checkMissingImagesAndFonts () /n
     * Called by KdenliveDoc::checkDocumentClips ()        /n
     * @return
     */
    bool hasErrorInClips();
    QString searchLuma(const QDir &dir, const QString &file) const;

private slots:
    void acceptDialog();
    void slotSearchClips();
    void slotEditItem(QTreeWidgetItem *item, int);
    void slotPlaceholders();
    void slotDeleteSelected();
    QString getProperty(const QDomElement &effect, const QString &name);
    void updateProperty(const QDomElement &effect, const QString &name, const QString &value);
    void setProperty(QDomElement &effect, const QString &name, const QString &value);
    /** @brief Check if images and fonts in this clip exists, returns a list of images that do exist so we don't check twice. */
    void checkMissingImagesAndFonts(const QStringList &images, const QStringList &fonts, const QString &id, const QString &baseClip);
    void slotCheckButtons();

private:
    QUrl m_url;
    QDomDocument m_doc;
    Ui::MissingClips_UI m_ui;
    QDialog *m_dialog;
    QPair<QString, QString> m_rootReplacement;
    QString searchPathRecursively(const QDir &dir, const QString &fileName, ClipType::ProducerType type = ClipType::Unknown) const;
    QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash, const QString &fileName) const;
    void checkStatus();
    QMap<QString, QString> m_missingTitleImages;
    QMap<QString, QString> m_missingTitleFonts;
    QList<QDomElement> m_missingClips;
    QStringList m_missingFilters;
    QStringList m_missingFonts;
    QStringList m_safeImages;
    QStringList m_safeFonts;
    QStringList m_missingProxyIds;

    void fixClipItem(QTreeWidgetItem *child, const QDomNodeList &producers, const QDomNodeList &trans);
    void fixSourceClipItem(QTreeWidgetItem *child, const QDomNodeList &producers);
    void fixProxyClip(const QString &id, const QString &oldUrl, const QString &newUrl, const QDomNodeList &producers);
    /** @brief Returns list of transitions containing luma files */
    QMap<QString, QString> getLumaPairs() const;
};

#endif
