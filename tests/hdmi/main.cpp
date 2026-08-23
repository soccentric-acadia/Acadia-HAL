#include "test_runner.h"
#include "hdmi_lib/hdmi_lib.h"

int main(int argc, char* argv[])
{
    return runTestApp<HdmiTesting>("HdmiTesting", argc, argv);
}
