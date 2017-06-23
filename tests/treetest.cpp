#include "catch.hpp"

#include <QColor>
#include <QDebug>
#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#define private public
#define protected public
#include "abstractmodel/treeitem.hpp"
#include "abstractmodel/abstracttreemodel.hpp"


TEST_CASE("Basic tree testing", "[TreeModel]")
{
    auto model = AbstractTreeModel::construct();

    REQUIRE(model->rowCount() == 0);

    SECTION("Item creation Test")
    {
        auto item = TreeItem::construct(QList<QVariant>{QString("test")}, model);
        int id = item->getId();
        REQUIRE(item->depth() == 0);

        // check that a valid Id has been assigned
        REQUIRE(id != -1);

        // check that we can query the item in the model
        REQUIRE(model->getItemById(id) == item);
        REQUIRE(model->m_allItems.size() == 1);


        // Assign this to a parent
        model->getRoot()->appendChild(item);
        REQUIRE(item->depth() == 1);
        REQUIRE(model->rowCount() == 1);
        qDebug() << (void*)model->getRoot().get() << (void*)item.get();
        qDebug() << model->getIndexFromItem(item);
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 0);
        REQUIRE(model->m_allItems.size() == 1);

        // Retrieve data member
        REQUIRE(model->data(model->getIndexFromItem(item), 0) == QStringLiteral("test"));


        // Try joint creation / assignation
        auto item2 = item->appendChild(QList<QVariant>{QString("test2")});
        REQUIRE(item->depth() == 1);
        REQUIRE(item2->depth() == 2);
        REQUIRE(model->rowCount() == 1);
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 1);
        REQUIRE(model->rowCount(model->getIndexFromItem(item2)) == 0);
        REQUIRE(model->m_allItems.size() == 2);
        REQUIRE(model->data(model->getIndexFromItem(item2), 0) == QStringLiteral("test2"));

    }
}
