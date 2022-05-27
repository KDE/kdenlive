/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"
#include "utils/qcolorutils.h"
#include <QDomElement>
#include <QWidget>

class ColorWheel;
class FlowLayout;

/**
 * @class LumaLiftGainParam
 * @brief Provides options to choose 3 colors.
 * @author Jean-Baptiste Mardelle
 */
class LumaLiftGainParam : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the widget.
     * @param text (optional) What the color will be used for
     * @param color (optional) initial color
     * @param alphaEnabled (optional) Should transparent colors be enabled */
    explicit LumaLiftGainParam(std::shared_ptr<AssetParameterModel> model, const QModelIndex &index, QWidget *parent);
    void updateEffect(QDomElement &effect);
    int miniHeight();

private:
    ColorWheel *m_lift;
    ColorWheel *m_gamma;
    ColorWheel *m_gain;
    FlowLayout *m_flowLayout;
    std::shared_ptr<AssetParameterModel> m_model;
    QPersistentModelIndex m_index;

protected:
    void resizeEvent(QResizeEvent *ev) override;

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void liftChanged(const NegQColor &color);
    void gammaChanged(const NegQColor &color);
    void gainChanged(const NegQColor &color);
    void valuesChanged(const QList<QModelIndex>, const QStringList &, bool);
    void updateHeight(int height);

public slots:
    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh(int pos);
};
