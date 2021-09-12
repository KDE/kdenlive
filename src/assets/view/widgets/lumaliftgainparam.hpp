/*
 *   SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
 *   SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 ***************************************************************************/

#ifndef LUMALIFTGAINPARAMWIDGET_H
#define LUMALIFTGAINPARAMWIDGET_H

#include "abstractparamwidget.hpp"
#include <QWidget>
#include <QDomElement>

class ColorWheel;
class FlowLayout;

/**
 * @class LumaLiftGainParam
 * @brief Provides options to choose 3 colors.
 * @author Jean-Baptiste Mardelle
 */
class LumaLiftGainParam : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the widget.
     * @param text (optional) What the color will be used for
     * @param color (optional) initial color
     * @param alphaEnabled (optional) Should transparent colors be enabled */
    explicit LumaLiftGainParam(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);
    void updateEffect(QDomElement &effect);

private:
    ColorWheel *m_lift;
    ColorWheel *m_gamma;
    ColorWheel *m_gain;
    FlowLayout *m_flowLayout;

protected:
    void resizeEvent(QResizeEvent *ev) override;

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void liftChanged();
    void gammaChanged();
    void gainChanged();

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;
};

#endif
