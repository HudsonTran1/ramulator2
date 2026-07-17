// test_frontend.cpp
#include "base/factory.h"
#include "base/config.h"
#include <iostream>

int main() {
    YAML::Node config = Ramulator::Config::parse_config_file("ramulator_config.yaml", {});
    auto* frontend = Ramulator::Factory::create_frontend(config);
    std::cout << (frontend ? "SUCCESS: frontend created\n" : "FAILED: frontend is null\n");
    return frontend ? 0 : 1;
}
