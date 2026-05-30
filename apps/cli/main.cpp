#include "app/CommandLineInterface.hpp"

#include <exception>
#include <iostream>

int main(
    int argc,
    char** argv
) {
    try {
        return nodo::app::CommandLineInterface::run(
            argc,
            argv
        );
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception.\n";
        return 1;
    }
}
