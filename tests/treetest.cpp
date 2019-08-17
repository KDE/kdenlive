#include "catch.hpp"

#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#define private public
#define protected public
#include "abstractmodel/abstracttreemodel.hpp"
#include "abstractmodel/treeitem.hpp"

TEST_CASE("Basic tree testing", "[TreeModel]")
{
    auto model = AbstractTreeModel::construct();

    REQUIRE(model->checkConsistency());
    REQUIRE(model->rowCount() == 0);

    SECTION("Item creation Test")
    {
        auto item = TreeItem::construct(QList<QVariant>{QString("test")}, model, false);
        int id = item->getId();
        REQUIRE(item->depth() == 0);
        REQUIRE(model->checkConsistency());

        // check that a valid Id has been assigned
        REQUIRE(id != -1);

        // check that the item is not yet registered (not valid parent)
        REQUIRE(model->m_allItems.size() == 1);

        // Assign this to a parent
        model->getRoot()->appendChild(item);
        REQUIRE(model->checkConsistency());
        // Now the item should be registered, we query it
        REQUIRE(model->m_allItems.size() == 2);
        REQUIRE(model->getItemById(id) == item);
        REQUIRE(item->depth() == 1);
        REQUIRE(model->rowCount() == 1);
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 0);

        // Retrieve data member
        REQUIRE(model->data(model->getIndexFromItem(item), 0) == QStringLiteral("test"));

        // Try joint creation / assignation
        auto item2 = item->appendChild(QList<QVariant>{QString("test2")});
        auto state = [&]() {
            REQUIRE(model->checkConsistency());
            REQUIRE(item->depth() == 1);
            REQUIRE(item2->depth() == 2);
            REQUIRE(model->rowCount() == 1);
            REQUIRE(item->row() == 0);
            REQUIRE(item2->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item2), 0) == QStringLiteral("test2"));
            REQUIRE(model->rowCount(model->getIndexFromItem(item2)) == 0);
        };
        state();
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 1);
        REQUIRE(model->m_allItems.size() == 3);

        // Add a second child to item to check if everything collapses
        auto item3 = item->appendChild(QList<QVariant>{QString("test3")});
        state();
        REQUIRE(model->rowCount(model->getIndexFromItem(item3)) == 0);
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 2);
        REQUIRE(model->m_allItems.size() == 4);
        REQUIRE(item3->depth() == 2);
        REQUIRE(item3->row() == 1);
        REQUIRE(model->data(model->getIndexFromItem(item3), 0) == QStringLiteral("test3"));
    }

    SECTION("Invalid moves")
    {
        auto item = model->getRoot()->appendChild(QList<QVariant>{QString("test")});
        auto state = [&]() {
            REQUIRE(model->checkConsistency());
            REQUIRE(model->rowCount() == 1);
            REQUIRE(item->depth() == 1);
            REQUIRE(item->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item), 0) == QStringLiteral("test"));
        };
        state();
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 0);

        // Try to move the root
        REQUIRE_FALSE(item->appendChild(model->getRoot()));
        state();
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 0);

        auto item2 = item->appendChild(QList<QVariant>{QString("test2")});
        auto item3 = item2->appendChild(QList<QVariant>{QString("test3")});
        auto item4 = item3->appendChild(QList<QVariant>{QString("test4")});
        auto state2 = [&]() {
            state();
            REQUIRE(item2->depth() == 2);
            REQUIRE(item2->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item2), 0) == QStringLiteral("test2"));
            REQUIRE(item3->depth() == 3);
            REQUIRE(item3->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item3), 0) == QStringLiteral("test3"));
            REQUIRE(item4->depth() == 4);
            REQUIRE(item4->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item4), 0) == QStringLiteral("test4"));
        };
        state2();

        // Try to make a loop
        REQUIRE_FALSE(item->changeParent(item3));
        state2();
        REQUIRE_FALSE(item->changeParent(item4));
        state2();

        // Try to append a child that already have a parent
        REQUIRE_FALSE(item->appendChild(item4));
        state2();

        // valid move
        REQUIRE(item4->changeParent(item2));
        REQUIRE(model->checkConsistency());
    }

    SECTION("Deregistration tests")
    {
        // we construct a non trivial structure
        auto item = model->getRoot()->appendChild(QList<QVariant>{QString("test")});
        auto item2 = item->appendChild(QList<QVariant>{QString("test2")});
        auto item3 = item2->appendChild(QList<QVariant>{QString("test3")});
        auto item4 = item3->appendChild(QList<QVariant>{QString("test4")});
        auto item5 = item2->appendChild(QList<QVariant>{QString("test5")});
        auto state = [&]() {
            REQUIRE(model->checkConsistency());
            REQUIRE(model->rowCount() == 1);
            REQUIRE(item->depth() == 1);
            REQUIRE(item->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item), 0) == QStringLiteral("test"));
            REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 1);
            REQUIRE(item2->depth() == 2);
            REQUIRE(item2->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item2), 0) == QStringLiteral("test2"));
            REQUIRE(model->rowCount(model->getIndexFromItem(item2)) == 2);
            REQUIRE(item3->depth() == 3);
            REQUIRE(item3->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item3), 0) == QStringLiteral("test3"));
            REQUIRE(model->rowCount(model->getIndexFromItem(item3)) == 1);
            REQUIRE(item4->depth() == 4);
            REQUIRE(item4->row() == 0);
            REQUIRE(model->data(model->getIndexFromItem(item4), 0) == QStringLiteral("test4"));
            REQUIRE(model->rowCount(model->getIndexFromItem(item4)) == 0);
            REQUIRE(item5->depth() == 3);
            REQUIRE(item5->row() == 1);
            REQUIRE(model->data(model->getIndexFromItem(item5), 0) == QStringLiteral("test5"));
            REQUIRE(model->rowCount(model->getIndexFromItem(item5)) == 0);
            REQUIRE(model->m_allItems.size() == 6);
            REQUIRE(item->isInModel());
            REQUIRE(item2->isInModel());
            REQUIRE(item3->isInModel());
            REQUIRE(item4->isInModel());
            REQUIRE(item5->isInModel());
        };
        state();

        // deregister the topmost item, should also deregister its children
        item->changeParent(std::shared_ptr<TreeItem>());
        REQUIRE(model->m_allItems.size() == 1);
        REQUIRE(model->rowCount() == 0);
        REQUIRE(!item->isInModel());
        REQUIRE(!item2->isInModel());
        REQUIRE(!item3->isInModel());
        REQUIRE(!item4->isInModel());
        REQUIRE(!item5->isInModel());

        // reinsert
        REQUIRE(model->getRoot()->appendChild(item));
        state();

        item2->removeChild(item5);
        REQUIRE(!item5->isInModel());
        REQUIRE(model->rowCount(model->getIndexFromItem(item2)) == 1);
        REQUIRE(model->m_allItems.size() == 5);

        // reinsert
        REQUIRE(item5->changeParent(item2));
        state();
    }
}
