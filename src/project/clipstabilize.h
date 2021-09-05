/***************************************************************************
 *   SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   SPDX-FileCopyrightText: 2011 Marco Gittler (marco@gitma.de)                  *
 *                                                                         *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 ***************************************************************************/

#ifndef CLIPSTABILIZE_H
#define CLIPSTABILIZE_H

#include "definitions.h"
#include "timecode.h"
#include "ui_clipstabilize_ui.h"
#include <QUrl>
#include <unordered_map>

class AssetParameterModel;
class AssetParameterView;

class ClipStabilize : public QDialog, public Ui::ClipStabilize_UI
{
    Q_OBJECT

public:
    explicit ClipStabilize(const std::vector<QString> &binIds, QString filterName, QWidget *parent = nullptr);
    ~ClipStabilize() override;
    /** @brief Should the generated clip be added to current project. */
    bool autoAddClip() const;
    /** @brief Return the filter parameters, filter name as value of "filter" entry. */
    std::unordered_map<QString, QVariant> filterParams() const;
    /** @brief Return the destination file or folder. */
    QString destination() const;
    /** @brief Return the job description. */
    QString desc() const;

    /* Return the name of the actual mlt filter used */
    QString filterName() const;
private slots:
    void slotValidate();

private:
    QString m_filtername;
    std::vector<QString> m_binIds;
    QVBoxLayout *m_vbox;
    Timecode m_tc;
    std::shared_ptr<AssetParameterModel> m_assetModel;
    std::unique_ptr<AssetParameterView> m_view;

signals:
    void addClip(const QUrl &url);
};

#endif
