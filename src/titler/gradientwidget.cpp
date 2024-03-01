/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "gradientwidget.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

#include <KConfigGroup>
#include <KSharedConfig>

GradientWidget::GradientWidget(const QMap<QString, QString> &gradients, int ix, QWidget *parent)
    : QDialog(parent)
    , Ui::GradientEdit_UI()
{
    setupUi(this);
    updatePreview();
    connect(color1_pos, &QAbstractSlider::valueChanged, this, &GradientWidget::updatePreview);
    connect(color2_pos, &QAbstractSlider::valueChanged, this, &GradientWidget::updatePreview);
    connect(angle, &QAbstractSlider::valueChanged, this, &GradientWidget::updatePreview);
    connect(color1, &KColorButton::changed, this, &GradientWidget::updatePreview);
    connect(color2, &KColorButton::changed, this, &GradientWidget::updatePreview);
    connect(add_gradient, SIGNAL(clicked()), this, SLOT(saveGradient()));
    connect(remove_gradient, &QAbstractButton::clicked, this, &GradientWidget::deleteGradient);
    QFontMetrics metrics(font());
    m_height = metrics.lineSpacing();
    gradient_list->setIconSize(QSize(6 * m_height, m_height));
    connect(gradient_list, &QListWidget::currentRowChanged, this, &GradientWidget::loadGradient);
    loadGradients(gradients);
    gradient_list->setCurrentRow(ix);
}

void GradientWidget::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updatePreview();
}

QString GradientWidget::gradientToString() const
{
    QStringList result;
    result << color1->color().name(QColor::HexArgb) << color2->color().name(QColor::HexArgb) << QString::number(color1_pos->value())
           << QString::number(color2_pos->value()) << QString::number(angle->value());
    return result.join(QLatin1Char(';'));
}

QLinearGradient GradientWidget::gradientFromString(const QString &str, int width, int height)
{
    QStringList values = str.split(QLatin1Char(';'));
    QLinearGradient gr;
    if (values.count() < 5) {
        // invalid gradient data
        return gr;
    }
    const QColor startColor(values.at(0));
    const QColor endColor(values.at(1));
    gr.setColorAt(values.at(2).toDouble() / 100, startColor);
    gr.setColorAt(values.at(3).toDouble() / 100, endColor);
    double angle = values.at(4).toDouble();
    if (angle <= 90) {
        gr.setStart(0, 0);
        gr.setFinalStop(width * qCos(qDegreesToRadians(angle)), height * qSin(qDegreesToRadians(angle)));
    } else {
        gr.setStart(width, 0);
        gr.setFinalStop(width + width * qCos(qDegreesToRadians(angle)), height * qSin(qDegreesToRadians(angle)));
    }
    return gr;
}

void GradientWidget::updatePreview()
{
    QPixmap p(preview->width(), preview->height());
    m_gradient = QLinearGradient();
    m_gradient.setColorAt(color1_pos->value() / 100.0, color1->color());
    m_gradient.setColorAt(color2_pos->value() / 100.0, color2->color());
    double ang = angle->value();
    if (ang <= 90) {
        m_gradient.setStart(0, 0);
        m_gradient.setFinalStop(p.width() / 2 * qCos(qDegreesToRadians(ang)), p.height() * qSin(qDegreesToRadians(ang)));
    } else {
        m_gradient.setStart(p.width() / 2, 0);
        m_gradient.setFinalStop(p.width() / 2 + (p.width() / 2) * qCos(qDegreesToRadians(ang)), p.height() * qSin(qDegreesToRadians(ang)));
    }
    // qCDebug(KDENLIVE_LOG)<<"* * * ANGLE: "<<angle->value()<<" = "<<p.height() * tan(angle->value() * 3.1415926 / 180.0);

    QLinearGradient copy = m_gradient;
    QPointF gradStart = m_gradient.start() + QPointF(p.width() / 2, 0);
    QPointF gradStop = m_gradient.finalStop() + QPointF(p.width() / 2, 0);
    copy.setStart(gradStart);
    copy.setFinalStop(gradStop);
    QBrush br(m_gradient);
    QBrush br2(copy);
    p.fill(Qt::transparent);
    QPainter painter(&p);
    painter.fillRect(0, 0, p.width() / 2, p.height(), br);
    QPainterPath path;
    QFont f = font();
    f.setPixelSize(p.height());
    int margin = p.height() / 8;
    path.addText(p.width() / 2 + 2 * margin, p.height() - margin, f, QStringLiteral("Ax"));
    painter.fillPath(path, br2);
    painter.end();
    preview->setPixmap(p);
    // save changes to currently active gradient
    QListWidgetItem *current = gradient_list->currentItem();
    if (!current) {
        return;
    }
    saveGradient(current->text());
}

int GradientWidget::selectedGradient() const
{
    return gradient_list->currentRow();
}

void GradientWidget::saveGradient(const QString &name)
{
    QPixmap pix(6 * m_height, m_height);
    pix.fill(Qt::transparent);
    m_gradient.setStart(0, pix.height() / 2);
    m_gradient.setFinalStop(pix.width(), pix.height() / 2);
    QPainter painter(&pix);
    painter.fillRect(0, 0, pix.width(), pix.height(), QBrush(m_gradient));
    painter.end();
    QIcon icon(pix);
    QListWidgetItem *item = nullptr;
    if (!name.isEmpty()) {
        item = gradient_list->currentItem();
        item->setIcon(icon);
    } else {
        // Create new gradient
        int ct = gradient_list->count();
        QStringList existing = getNames();
        QString test = i18n("Gradient %1", ct);
        while (existing.contains(test)) {
            ct++;
            test = i18n("Gradient %1", ct);
        }
        item = new QListWidgetItem(icon, test, gradient_list);
        item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }
    item->setData(Qt::UserRole, gradientToString());
}

QStringList GradientWidget::getNames() const
{
    QStringList result;
    result.reserve(gradient_list->count());
    for (int i = 0; i < gradient_list->count(); i++) {
        result << gradient_list->item(i)->text();
    }
    return result;
}

void GradientWidget::deleteGradient()
{
    QListWidgetItem *item = gradient_list->currentItem();
    delete item;
}

void GradientWidget::loadGradient()
{
    QListWidgetItem *item = gradient_list->currentItem();
    if (!item) {
        return;
    }
    QString grad_data = item->data(Qt::UserRole).toString();
    QStringList res = grad_data.split(QLatin1Char(';'));
    if (res.count() < 5) {
        // invalid gradient data
        return;
    }
    color1->setColor(QColor(res.at(0)));
    color2->setColor(QColor(res.at(1)));
    color1_pos->setValue(res.at(2).toInt());
    color2_pos->setValue(res.at(3).toInt());
    angle->setValue(res.at(4).toInt());
}

QMap<QString, QString> GradientWidget::gradients() const
{
    QMap<QString, QString> gradients;
    for (int i = 0; i < gradient_list->count(); i++) {
        gradients.insert(gradient_list->item(i)->text(), gradient_list->item(i)->data(Qt::UserRole).toString());
    }
    return gradients;
}

QList<QIcon> GradientWidget::icons() const
{
    QList<QIcon> icons;
    icons.reserve(gradient_list->count());
    for (int i = 0; i < gradient_list->count(); i++) {
        QPixmap pix = gradient_list->item(i)->icon().pixmap(6 * m_height, m_height);
        QIcon icon(pix.scaled(30, 30));
        icons << icon;
    }
    return icons;
}

void GradientWidget::loadGradients(QMap<QString, QString> gradients)
{
    gradient_list->clear();
    if (gradients.isEmpty()) {
        KSharedConfigPtr config = KSharedConfig::openConfig();
        KConfigGroup group(config, "TitleGradients");
        gradients = group.entryMap();
    }
    QMapIterator<QString, QString> k(gradients);
    while (k.hasNext()) {
        k.next();
        QPixmap pix(6 * m_height, m_height);
        pix.fill(Qt::transparent);
        QLinearGradient gr = gradientFromString(k.value(), pix.width(), pix.height());
        gr.setStart(0, pix.height() / 2);
        gr.setFinalStop(pix.width(), pix.height() / 2);
        QPainter painter(&pix);
        painter.fillRect(0, 0, pix.width(), pix.height(), QBrush(gr));
        painter.end();
        QIcon icon(pix);
        auto *item = new QListWidgetItem(icon, k.key(), gradient_list);
        item->setData(Qt::UserRole, k.value());
        item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }
}
