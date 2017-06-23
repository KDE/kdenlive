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
    }
}
