#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "core.h"
#include <QApplication>
#include <mlt++/MltFactory.h>
#include <mlt++/MltRepository.h>
/* This file is intended to remain empty.
Write your tests in a file with a name corresponding to what you're testing */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    std::unique_ptr<Mlt::Repository> repo(Mlt::Factory::init(nullptr));
    Core::build();

    int result = Catch::Session().run(argc, argv);

    // global clean-up...
    // delete repo;

    Mlt::Factory::close();
    return (result < 0xff ? result : 0xff);
}
