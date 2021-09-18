/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef GENERATORS_H
#define GENERATORS_H

#include "assets/model/assetparametermodel.hpp"
#include "assets/view/assetparameterview.hpp"
#include <QDialog>
#include <QMenu>
#include <QPair>
#include <QPixmap>

namespace Mlt {
class Producer;
}

class QLabel;
class QResizeEvent;
class ParameterContainer;
class TimecodeDisplay;

/** @class Generators
    @brief Base class for clip generators.
 */
class Generators : public QDialog
{
    Q_OBJECT

public:
    explicit Generators(const QString &path, QWidget *parent = nullptr);
    ~Generators() override;

    static void getGenerators(const QStringList &producers, QMenu *menu);
    static QPair<QString, QString> parseGenerator(const QString &path, const QStringList &producers);
    QUrl getSavedClip(QString path = QString());

    void resizeEvent(QResizeEvent *event) override;

private:
    Mlt::Producer *m_producer;
    TimecodeDisplay *m_timePos;
    ParameterContainer *m_container;
    AssetParameterView *m_view;
    std::shared_ptr<AssetParameterModel> m_assetModel;
    QLabel *m_preview;
    QPixmap m_pixmap;

private slots:
    void updateProducer();
    void updateDuration(int duration);
};

#endif
