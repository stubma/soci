//
// Copyright (C) 2004-2006 Maciej Sobczak, Stephen Hutton
// MySQL backend copyright (C) 2006 Pawel Aleksander Fedorynski
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include "soci/soci.h"
#include "soci/mysql/soci-mysql.h"
#include "mysql/test-mysql.h"
#include <string.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <ctime>
#include <ciso646>
#include <cstdlib>
#include <mysqld_error.h>
#include <errmsg.h>

std::string connectString;
backend_factory const &backEnd = *soci::factory_mysql();

int main(int argc, char** argv)
{
    connectString = "host=127.0.0.1 db=test user=root password=asdfgUIOPNjiJIgegiIGef charset=utf8mb4";
    // if (argc >= 2)
    // {
    //     connectString = "mysql://host=127.0.0.1 db=EOS user=root password=asdfgUIOPNjiJIgegiIGef charset=utf8mb4";

    //     // Replace the connect string with the process name to ensure that
    //     // CATCH uses the correct name in its messages.
    //     argv[1] = argv[0];

    //     argc--;
    //     argv++;
    // }
    // else
    // {
    //     std::cout << "usage: " << argv[0]
    //         << " connectstring [test-arguments...]\n"
    //         << "example: " << argv[0]
    //         << " \"dbname=test user=root password=\'Ala ma kota\'\"\n";
    //     std::exit(1);
    // }

    test_context tc(backEnd, connectString);

    return Catch::Session().run(argc, argv);
}
