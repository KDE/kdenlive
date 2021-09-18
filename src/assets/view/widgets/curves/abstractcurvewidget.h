/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef ABSTRACTCURVEWIDGET_H
#define ABSTRACTCURVEWIDGET_H

#include "bezier/bpoint.h"
#include <QWidget>
#include <memory>

#include <QPainter>

/** State of a point being moved */
enum class State_t { NORMAL, DRAG };

/** @class __dummy_AbstractCurveWidget
    @brief This class is a workaround to be able to use templating in the actual class
    Note that Qt doesn't support templated signals, so we have to define a signal for
    all possible Point type
*/
class __dummy_AbstractCurveWidget : public QWidget
{
    Q_OBJECT
public:
    __dummy_AbstractCurveWidget(QWidget *parent)
        : QWidget(parent)
    {
    }

signals:
    /**
     * Emitted whenever a control point has changed position.
     */
    void modified();
    /**
       Signal sent when the current point changes. The point is returned, as well as a flag that determines if the point is the first or last.
    */
    void currentPoint(const QPointF &p, bool extremal);
    void currentPoint(const BPoint &p, bool extremal);
    void resized(const QSize &s);

public slots:
    /** @brief Delete current spline point if it is not a extremal point (first or last)
     */
    virtual void slotDeleteCurrentPoint() = 0;
    virtual void slotZoomIn() = 0;
    virtual void slotZoomOut() = 0;
    virtual void reset() = 0;
};

/** @brief Base class of all the widgets representing a curve of points

 */
template <typename Curve_t> class AbstractCurveWidget : public __dummy_AbstractCurveWidget
{
public:
    typedef typename Curve_t::Point_t Point_t; // NOLINT
    ~AbstractCurveWidget() override = default;

    /** @param parent Optional parent of the widget
     */
    AbstractCurveWidget(QWidget *parent = nullptr);

    /** @brief Returns whether the points are controlled with additional handles
     */
    virtual bool hasHandles() { return false; }

    /** @brief Sets the maximal number of points of the curve
     */
    void setMaxPoints(int max);

    /** @brief Sets the background pixmap to @param pixmap.
        The background pixmap will be drawn under the grid and the curve*/
    void setPixmap(const QPixmap &pixmap);

    /** @brief Number of lines used in grid. */
    int gridLines() const;

    /** @brief Sets the number of grid lines to draw (in one direction) to @param lines. */
    void setGridLines(int lines);

    /** @brief Constructs the curve from @param string
     */
    void setFromString(const QString &string);

    /** @brief Resets the curve to an empty one
     */
    void reset() override;

    /** @brief Returns a string corresponding to the curve
     */
    QString toString();

    /** @brief Replaces current point with @param p (index stays the same).
     * @param final (default = true) emit signal modified? */
    void updateCurrentPoint(const Point_t &p, bool final = true);

    /** @brief Returns the selected point or else empty point. */
    Point_t getCurrentPoint();

    /** @brief Returns the list of all the points. */
    virtual QList<Point_t> getPoints() const = 0;

public:
    /** @brief Delete current spline point if it is not a extremal point (first or last)
     */
    void slotDeleteCurrentPoint() override;
    void slotZoomIn() override;
    void slotZoomOut() override;

protected:
    void paintBackground(QPainter *p);
    int heightForWidth(int w) const override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *) override;
    /**
       Utility function to check if current selected point is the first or the last
    */
    bool isCurrentPointExtremal();

    int m_zoomLevel{0};
    int m_gridLines{3};
    /** Background */
    QPixmap m_pixmap;
    /** A copy of m_pixmap but scaled to fit the size of the edit region */
    std::shared_ptr<QPixmap> m_pixmapCache;
    /** Whether we have to regenerate the pixmap cache because the pixmap or the size of the edit region changed. */
    bool m_pixmapIsDirty{true};

    int m_currentPointIndex{-1};
    int m_maxPoints{1000000};
    int m_wWidth, m_wHeight;

    State_t m_state{State_t::NORMAL};
    Curve_t m_curve;

    double m_grabRadius{10};
};

#include "abstractcurvewidget.ipp"

#endif
