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
        auto item = TreeItem::construct(QList<QVariant>{QString("test")}, model, false);
        int id = item->getId();
        REQUIRE(item->depth() == 0);

        // check that a valid Id has been assigned
        REQUIRE(id != -1);

        // check that the item is not yet registered (not valid parent)
        REQUIRE(model->m_allItems.size() == 0);


        // Assign this to a parent
        model->getRoot()->appendChild(item);
        // Now the item should be registered, we query it
        REQUIRE(model->m_allItems.size() == 1);
        REQUIRE(model->getItemById(id) == item);
        REQUIRE(item->depth() == 1);
        REQUIRE(model->rowCount() == 1);
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 0);

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
