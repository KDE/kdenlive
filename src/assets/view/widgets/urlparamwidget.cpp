/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "urlparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

#include <KUrlRequester>
#include <kio_version.h>

UrlParamWidget::UrlParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

    // setup the comment
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    labelComment->setText(comment);
    setToolTip(comment);
    labelComment->setHidden(true);
    QString filter = m_model->data(m_index, AssetParameterModel::FilterRole).toString();
    if (!filter.isEmpty()) {
#if KIO_VERSION >= QT_VERSION_CHECK(5, 108, 0)
        urlwidget->setNameFilter(filter);
#else
        // regular expression copied from QPlatformFileDialogHelper
        QStringList filters = filter.split(QStringLiteral(";;"), Qt::SkipEmptyParts);
        const QRegularExpression regexp(QStringLiteral("^(.*)\\(([a-zA-Z0-9_.,*? +;#\\-\\[\\]@\\{\\}/!<>\\$%&=^~:\\|]*)\\)$"));
        QStringList result;
        for (const QString &filter : filters) {
            QRegularExpressionMatch match;
            filter.indexOf(regexp, 0, &match);
            if (match.hasMatch()) {
                result.append(match.capturedView(2).trimmed() + QLatin1Char('|') + match.capturedView(1).trimmed());
            } else {
                result.append(filter);
            }
        }

        urlwidget->setFilter(result.join(QLatin1Char('\n')));
#endif
    }
    QString mode = m_model->data(m_index, AssetParameterModel::ModeRole).toString();
    if (mode == QLatin1String("save")) {
        urlwidget->setAcceptMode(QFileDialog::AcceptSave);
        urlwidget->setMode(KFile::File);
    }
    slotRefresh();

    // setup the name
    label->setText(m_model->data(m_index, Qt::DisplayRole).toString());
    setMinimumHeight(urlwidget->sizeHint().height());

    // set check state
    slotRefresh();

    // Q_EMIT the signal of the base class when appropriate
    connect(this->urlwidget, &KUrlRequester::textChanged, this, [this]() {
        QFileInfo info(urlwidget->url().toLocalFile());
        if (info.exists() && info.isFile()) {
            Q_EMIT valueChanged(m_index, this->urlwidget->url().toLocalFile(), true);
        }
    });
}

void UrlParamWidget::slotShowComment(bool show)
{
    if (!labelComment->text().isEmpty()) {
        labelComment->setVisible(show);
    }
}

void UrlParamWidget::slotRefresh()
{
    const QSignalBlocker bk(urlwidget);
    urlwidget->setUrl(QUrl::fromLocalFile(m_model->data(m_index, AssetParameterModel::ValueRole).toString()));
}
