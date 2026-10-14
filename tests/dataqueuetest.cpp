/*
    SPDX-FileCopyrightText: 2026 Erich Seifert <dev@erichseifert.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"

#include "monitor/scopes/dataqueue.h"

#include <QThread>

#include <atomic>

TEST_CASE("DataQueue", "[DataQueue]")
{
    SECTION("tryPop does not block on an empty queue")
    {
        DataQueue<int> queue(3, DataQueue<int>::OverflowModeDiscardOldest);
        int item = -1;
        CHECK_FALSE(queue.tryPop(item));
        CHECK(item == -1);
        queue.push(1);
        queue.push(2);
        REQUIRE(queue.tryPop(item));
        CHECK(item == 1);
        REQUIRE(queue.tryPop(item));
        CHECK(item == 2);
        CHECK_FALSE(queue.tryPop(item));
    }

    SECTION("Oldest items are discarded on overflow")
    {
        DataQueue<int> queue(3, DataQueue<int>::OverflowModeDiscardOldest);
        for (int i = 1; i <= 5; i++) {
            queue.push(i);
        }
        REQUIRE(queue.count() == 3);
        CHECK(queue.pop() == 3);
    }

    SECTION("clear empties the queue")
    {
        DataQueue<int> queue(3, DataQueue<int>::OverflowModeDiscardOldest);
        queue.push(1);
        queue.push(2);
        queue.clear();
        CHECK(queue.count() == 0);
        int item = -1;
        CHECK_FALSE(queue.tryPop(item));
    }

    SECTION("Clearing while another thread consumes does not block")
    {
        // The scopes clear their queue from the GUI thread while the refresh thread
        // empties it. With count() followed by pop(), one of them could wait forever.
        DataQueue<int> queue(3, DataQueue<int>::OverflowModeDiscardOldest);
        std::atomic_bool stop{false};
        std::atomic_int consumed{0};
        QThread *consumer = QThread::create([&]() {
            int item;
            while (!stop) {
                while (queue.tryPop(item)) {
                    consumed++;
                }
            }
        });
        consumer->start();
        for (int i = 0; i < 100000; i++) {
            queue.push(i);
            queue.clear();
        }
        stop = true;
        REQUIRE(consumer->wait(5000));
        delete consumer;
        CHECK(queue.count() == 0);
    }
}
