#include "test_runner.h"
#include "fan_lib/fan_lib.h"

int main(int argc, char* argv[])
{
    return runTestApp<FanTesting>("FanTesting", argc, argv);
}
