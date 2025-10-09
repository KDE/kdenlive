/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kddocksetup.h"
#include "core.h"

#include <QPainter>

class MyTabBar : public KDDockWidgets::QtWidgets::TabBar
{
public:
    explicit MyTabBar(KDDockWidgets::Core::TabBar *controller, KDDockWidgets::Core::View *parent = nullptr)
        : KDDockWidgets::QtWidgets::TabBar(controller, KDDockWidgets::QtCommon::View_qt::asQWidget(parent))
    {
        auto parentWidget = KDDockWidgets::QtCommon::View_qt::asQWidget(parent);
        setProperty("_breeze_force_frame", false);
        setDocumentMode(true);
        parentWidget->setProperty("_breeze_force_frame", false);
        parentWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(parentWidget, &QWidget::customContextMenuRequested, []() { Q_EMIT pCore.get()->switchTitleBars(); });
    }
};

class MyGroup : public KDDockWidgets::QtWidgets::Group
{
public:
    explicit MyGroup(KDDockWidgets::Core::Group *controller, KDDockWidgets::Core::View *parent = nullptr)
        : KDDockWidgets::QtWidgets::Group(controller, KDDockWidgets::QtCommon::View_qt::asQWidget(parent))
    {
    }
    void paintEvent(QPaintEvent *) override {}
};

class MyTitleBar : public KDDockWidgets::QtWidgets::TitleBar
{
public:
    explicit MyTitleBar(KDDockWidgets::Core::TitleBar *controller, KDDockWidgets::Core::View *parent = nullptr)
        : KDDockWidgets::QtWidgets::TitleBar(controller, parent)
        , m_controller(controller)
    {
        connect(pCore.get(), &Core::hideBars, this, [this](bool hide) {
            if (!hide && (m_controller->dockWidgets().size() > 1 || m_controller->isFloating())) {
                // Don't show title bar when there are tabbed widgets
                return;
            }
            setVisible(!hide);
        });
    }

    ~MyTitleBar() override;

    /*void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        QPen pen(Qt::black);
        const QColor focusedBackgroundColor = Qt::yellow;
        const QColor backgroundColor = focusedBackgroundColor.darker(115);
        QBrush brush(m_controller->isFocused() ? focusedBackgroundColor : backgroundColor);
        pen.setWidth(4);
        p.setPen(pen);
        p.setBrush(brush);
        p.drawRect(rect().adjusted(4, 4, -4, -4));
        QFont f = qGuiApp->font();
        f.setPixelSize(30);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QPoint(10, 40), m_controller->title());
    }*/

    // Not needed to override. Just here to illustrate setHideDisabledButtons()
    void init() override
    {
        // For demo purposes, we're hiding the close button if it's disabled (non-closable dock widget)
        // Affects dock #0 when running: ./bin/qtwidgets_dockwidgets -n -p
        m_controller->setHideDisabledButtons(KDDockWidgets::TitleBarButtonType::Close);

        // But if you do override init(), never forget to call the base method:
        KDDockWidgets::QtWidgets::TitleBar::init();
    }

private:
    KDDockWidgets::Core::TitleBar *const m_controller;
};

MyTitleBar::~MyTitleBar() = default;

class MySeparator : public KDDockWidgets::QtWidgets::Separator
{
public:
    explicit MySeparator(KDDockWidgets::Core::Separator *controller, KDDockWidgets::Core::View *parent)
        : KDDockWidgets::QtWidgets::Separator(controller, parent)
        , m_controller(controller)
    {
    }

    ~MySeparator() override;

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

MySeparator::~MySeparator() = default;

KDDockWidgets::Core::View *CustomWidgetFactory::createTitleBar(KDDockWidgets::Core::TitleBar *controller, KDDockWidgets::Core::View *parent) const
{
    // Feel free to return MyTitleBar_CSS here instead, but just for education purposes!
    return new MyTitleBar(controller, parent);
}

KDDockWidgets::Core::View *CustomWidgetFactory::createGroup(KDDockWidgets::Core::Group *controller, KDDockWidgets::Core::View *parent) const
{
    // Feel free to return MyTitleBar_CSS here instead, but just for education purposes!
    return new MyGroup(controller, parent);
}

KDDockWidgets::Core::View *CustomWidgetFactory::createSeparator(KDDockWidgets::Core::Separator *controller, KDDockWidgets::Core::View *parent) const
{
    return new MySeparator(controller, parent);
}

KDDockWidgets::Core::View *CustomWidgetFactory::createTabBar(KDDockWidgets::Core::TabBar *controller, KDDockWidgets::Core::View *parent) const
{
    return new MyTabBar(controller, parent);
}
