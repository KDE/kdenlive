/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kddocksetup.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <QPainter>

class KdenliveDockTabBar : public KDDockWidgets::QtWidgets::TabBar
{
public:
    explicit KdenliveDockTabBar(KDDockWidgets::Core::TabBar *controller, KDDockWidgets::Core::View *parent = nullptr)
        : KDDockWidgets::QtWidgets::TabBar(controller, KDDockWidgets::QtCommon::View_qt::asQWidget(parent))
    {
        auto parentWidget = KDDockWidgets::QtCommon::View_qt::asQWidget(parent);
        setProperty("_breeze_force_frame", false);
        setDocumentMode(true);
        parentWidget->setProperty("_breeze_force_frame", false);
        setContextMenuPolicy(Qt::CustomContextMenu);
        connect(this, &QWidget::customContextMenuRequested, []() { Q_EMIT pCore.get()->switchTitleBars(); });
        connect(this, &KDDockWidgets::QtWidgets::TabBar::countChanged, [&]() {
            if (!KdenliveSettings::showtitlebars()) {
                pCore->startHideBarsTimer();
            }
        });
    }
};

class KdenliveDockGroup : public KDDockWidgets::QtWidgets::Group
{
public:
    explicit KdenliveDockGroup(KDDockWidgets::Core::Group *controller, KDDockWidgets::Core::View *parent = nullptr)
        : KDDockWidgets::QtWidgets::Group(controller, KDDockWidgets::QtCommon::View_qt::asQWidget(parent))
    {
    }
    void paintEvent(QPaintEvent *) override {}
};

class KdenliveDockStack : public KDDockWidgets::QtWidgets::Stack
{
public:
    explicit KdenliveDockStack(KDDockWidgets::Core::Stack *controller, KDDockWidgets::Core::View *parent = nullptr)
        : KDDockWidgets::QtWidgets::Stack(controller, KDDockWidgets::QtCommon::View_qt::asQWidget(parent))
    {
    }
    void paintEvent(QPaintEvent *) override {}
};

class KdenliveDockTitleBar : public KDDockWidgets::QtWidgets::TitleBar
{
public:
    explicit KdenliveDockTitleBar(KDDockWidgets::Core::TitleBar *controller, KDDockWidgets::Core::View *parent = nullptr)
        : KDDockWidgets::QtWidgets::TitleBar(controller, parent)
        , m_controller(controller)
    {
        connect(pCore.get(), &Core::hideBars, this, [this](bool hide) {
            if (!hide) {
                if (m_controller->dockWidgets().size() > 1) {
                    // Don't show title bar when there are tabbed widgets
                    return;
                }
                auto parentWidget = m_controller->view()->parentView();
                if (parentWidget && parentWidget->asFloatingWindowController() != nullptr) {
                    // Floating window, don't show as we already have the widget titlebar
                    return;
                }
            }
            setVisible(!hide);
        });
    }

private:
    KDDockWidgets::Core::TitleBar *const m_controller;
};

class KdenliveDockSeparator : public KDDockWidgets::QtWidgets::Separator
{
public:
    explicit KdenliveDockSeparator(KDDockWidgets::Core::Separator *controller, KDDockWidgets::Core::View *parent)
        : KDDockWidgets::QtWidgets::Separator(controller, parent)
        , m_controller(controller)
    {
    }

    ~KdenliveDockSeparator() override;

    void enterEvent(KDDockWidgets::Qt5Qt6Compat::QEnterEvent *event) override
    {
        hovered = true;
        KDDockWidgets::QtWidgets::Separator::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override
    {
        hovered = false;
        KDDockWidgets::QtWidgets::Separator::leaveEvent(event);
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        QColor separatorColor = hovered ? palette().highlight().color() : palette().midlight().color();
        if (hovered) {
            separatorColor.setAlpha(128);
        }
        QPen pen(separatorColor);
        pen.setWidth(2);
        if (m_controller->isVertical()) {
            // Vertical rect
            p.fillRect(QWidget::rect(), palette().window());
            p.setPen(pen);
            p.drawLine(QWidget::rect().x(), QWidget::rect().y() + QWidget::rect().height() / 2, QWidget::rect().right(),
                       QWidget::rect().y() + QWidget::rect().height() / 2);
        } else {
            // Horizontal rect
            p.fillRect(QWidget::rect(), palette().window());
            p.setPen(pen);
            p.drawLine(QWidget::rect().x() + QWidget::rect().width() / 2, QWidget::rect().top(), QWidget::rect().x() + QWidget::rect().width() / 2,
                       QWidget::rect().bottom());
        }
    }

private:
    KDDockWidgets::Core::Separator *const m_controller;
    bool hovered{false};
};

KdenliveDockSeparator::~KdenliveDockSeparator() = default;

KDDockWidgets::Core::View *CustomWidgetFactory::createTitleBar(KDDockWidgets::Core::TitleBar *controller, KDDockWidgets::Core::View *parent) const
{
    return new KdenliveDockTitleBar(controller, parent);
}

KDDockWidgets::Core::View *CustomWidgetFactory::createGroup(KDDockWidgets::Core::Group *controller, KDDockWidgets::Core::View *parent) const
{
    return new KdenliveDockGroup(controller, parent);
}

KDDockWidgets::Core::View *CustomWidgetFactory::createStack(KDDockWidgets::Core::Stack *controller, KDDockWidgets::Core::View *parent) const
{
    return new KdenliveDockStack(controller, parent);
}

KDDockWidgets::Core::View *CustomWidgetFactory::createSeparator(KDDockWidgets::Core::Separator *controller, KDDockWidgets::Core::View *parent) const
{
    return new KdenliveDockSeparator(controller, parent);
}

KDDockWidgets::Core::View *CustomWidgetFactory::createTabBar(KDDockWidgets::Core::TabBar *controller, KDDockWidgets::Core::View *parent) const
{
    return new KdenliveDockTabBar(controller, parent);
}
