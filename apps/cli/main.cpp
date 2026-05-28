#include "app/DemoScenario.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        return nodo::app::runBlockchainFoundationDemo();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception.\n";
        return 1;
    }
}