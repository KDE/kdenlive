#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <mlt++/MltRepository.h>
#include <mlt++/MltFactory.h>
/* This file is intended to remain empty.
Write your tests in a file with a name corresponding to what you're testing */

int main( int argc, char* argv[] )
{
    std::unique_ptr<Mlt::Repository> repo (Mlt::Factory::init( NULL ));

    int result = Catch::Session().run( argc, argv );

    // global clean-up...
    //delete repo;

    return ( result < 0xff ? result : 0xff );
}
