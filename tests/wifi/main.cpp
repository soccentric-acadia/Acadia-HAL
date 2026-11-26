#include "test_runner.h"
#include "wifi_lib/wifi_lib.h"

int main(int argc, char* argv[])
{
    return runTestApp<WifiTesting>("WifiTesting", argc, argv);
}
