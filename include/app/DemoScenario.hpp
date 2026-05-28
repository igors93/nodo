#ifndef NODO_APP_DEMO_SCENARIO_HPP
#define NODO_APP_DEMO_SCENARIO_HPP

namespace nodo::app {

/*
 * Runs the current development scenario for Nodo.
 *
 * This keeps apps/cli/main.cpp small and prevents the CLI entry point
 * from becoming responsible for blockchain construction details.
 */
int runBlockchainFoundationDemo();

} // namespace nodo::app

#endif