/*
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#include "abstractmodel/abstracttreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include "effects/effectlist/model/effecttreemodel.hpp"
#include "effects/effectlist/model/effectfilter.hpp"

TEST_CASE("Basic tree testing", "[TreeModel]")
{
    auto model = AbstractTreeModel::construct();

    REQUIRE(KdenliveTests::checkModelConsistency(model));
    REQUIRE(model->rowCount() == 0);

    SECTION("Item creation Test")
    {
        auto item = TreeItem::construct(QList<QVariant>{QStringLiteral("test")}, model, false);
        int id = item->getId();
        REQUIRE(item->depth() == 0);
        REQUIRE(KdenliveTests::checkModelConsistency(model));

        // check that a valid Id has been assigned
        REQUIRE(id != -1);

        // check that the item is not yet registered (not valid parent)
        REQUIRE(KdenliveTests::modelSize(model) == 1);

        // Assign this to a parent
        model->getRoot()->appendChild(item);
        REQUIRE(KdenliveTests::checkModelConsistency(model));
        // Now the item should be registered, we query it
        REQUIRE(KdenliveTests::modelSize(model) == 2);
        REQUIRE(model->getItemById(id) == item);
        REQUIRE(item->depth() == 1);
        REQUIRE(model->rowCount() == 1);
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 0);

        // Retrieve data member
        REQUIRE(model->data(model->getIndexFromItem(item), 0) == QStringLiteral("test"));

        // Try joint creation / assignation
        auto item2 = item->appendChild(QList<QVariant>{QStringLiteral("test2")});
        auto state = [&]() {
            REQUIRE(KdenliveTests::checkModelConsistency(model));
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
        REQUIRE(KdenliveTests::modelSize(model) == 3);

        // Add a second child to item to check if everything collapses
        auto item3 = item->appendChild(QList<QVariant>{QStringLiteral("test3")});
        state();
        REQUIRE(model->rowCount(model->getIndexFromItem(item3)) == 0);
        REQUIRE(model->rowCount(model->getIndexFromItem(item)) == 2);
        REQUIRE(KdenliveTests::modelSize(model) == 4);
        REQUIRE(item3->depth() == 2);
        REQUIRE(item3->row() == 1);
        REQUIRE(model->data(model->getIndexFromItem(item3), 0) == QStringLiteral("test3"));
    }

    SECTION("Invalid moves")
    {
        auto item = model->getRoot()->appendChild(QList<QVariant>{QStringLiteral("test")});
        auto state = [&]() {
            REQUIRE(KdenliveTests::checkModelConsistency(model));
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

        auto item2 = item->appendChild(QList<QVariant>{QStringLiteral("test2")});
        auto item3 = item2->appendChild(QList<QVariant>{QStringLiteral("test3")});
        auto item4 = item3->appendChild(QList<QVariant>{QStringLiteral("test4")});
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
        REQUIRE(KdenliveTests::checkModelConsistency(model));
    }

    SECTION("Deregistration tests")
    {
        // we construct a non trivial structure
        auto item = model->getRoot()->appendChild(QList<QVariant>{QStringLiteral("test")});
        auto item2 = item->appendChild(QList<QVariant>{QStringLiteral("test2")});
        auto item3 = item2->appendChild(QList<QVariant>{QStringLiteral("test3")});
        auto item4 = item3->appendChild(QList<QVariant>{QStringLiteral("test4")});
        auto item5 = item2->appendChild(QList<QVariant>{QStringLiteral("test5")});
        auto state = [&]() {
            REQUIRE(KdenliveTests::checkModelConsistency(model));
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
            REQUIRE(KdenliveTests::modelSize(model) == 6);
            REQUIRE(item->isInModel());
            REQUIRE(item2->isInModel());
            REQUIRE(item3->isInModel());
            REQUIRE(item4->isInModel());
            REQUIRE(item5->isInModel());
        };
        state();

        // deregister the topmost item, should also deregister its children
        item->changeParent(std::shared_ptr<TreeItem>());
        REQUIRE(KdenliveTests::modelSize(model) == 1);
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
        REQUIRE(KdenliveTests::modelSize(model) == 5);

        // reinsert
        REQUIRE(item5->changeParent(item2));
        state();
    }
}

// Tests the logic for matching the user-supplied search string against the list
// of items. The actual logic is in AssetFilter but since it's an abstract
// class, we test EffectFilter instead.
TEST_CASE("Effect filter text-matching logic")
{
    auto model = EffectTreeModel::construct("", nullptr);
    EffectFilter filter{nullptr};
    // rootData copied from effecttreemodel.cpp
    QList<QVariant> rootData{"Name", "ID", "Type", "isFav"};

    SECTION("Handles basic alphanum search")
    {
        auto item = TreeItem::construct(rootData, model, true);
        item->setData(AssetTreeModel::IdCol, "Hello World");
        item->setData(AssetTreeModel::NameCol, "This is K-denlive");

        filter.setFilterName(true, "wORL"); // codespell:ignore worl
        CHECK(KdenliveTests::effectFilterName(filter, item) == true);

        // should not search across spaces
        filter.setFilterName(true, "Thisis");
        CHECK(KdenliveTests::effectFilterName(filter, item) == false);

        // should ignore punctuation
        filter.setFilterName(true, "Kden");
        CHECK(KdenliveTests::effectFilterName(filter, item) == true);
    }

    // Tests for bug 432699 where filtering the effects list in Chinese fails
    // because the filter only works for ascii alphanum characters.
    SECTION("Handles Chinese/Japanese search")
    {
        auto item = TreeItem::construct(rootData, model, true);
        item->setData(AssetTreeModel::IdCol, "静音");
        item->setData(AssetTreeModel::NameCol, "ミュート");

        filter.setFilterName(true, "音");
        CHECK(KdenliveTests::effectFilterName(filter, item) == true);
        filter.setFilterName(true, "默");
        CHECK(KdenliveTests::effectFilterName(filter, item) == false);

        // should ignore punctuation
        filter.setFilterName(true, "ミュ");
        CHECK(KdenliveTests::effectFilterName(filter, item) == true);
    }

    SECTION("Ignores diacritics")
    {
        auto item = TreeItem::construct(rootData, model, true);
        item->setData(AssetTreeModel::IdCol, "rgb parade");
        item->setData(AssetTreeModel::NameCol, "Défilé RVB");

        // should be able to search with and without diacritics
        filter.setFilterName(true, "defile");
        CHECK(KdenliveTests::effectFilterName(filter, item) == true);
        filter.setFilterName(true, "ilé");
        CHECK(KdenliveTests::effectFilterName(filter, item) == true);
    }
}
