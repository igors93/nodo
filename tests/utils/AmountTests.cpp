#include "utils/Amount.hpp"

#include <cassert>
#include <stdexcept>

namespace {

using nodo::utils::Amount;

void testFromNodoZero() {
  const Amount a = Amount::fromNodo(0);
  assert(a.isZero());
  assert(a.rawUnits() == 0);
}

void testFromNodoSmallValue() {
  const Amount a = Amount::fromNodo(1);
  assert(a.rawUnits() == Amount::UNITS_PER_NODO);
  assert(a.isPositive());
}

void testFromNodoMaxSafeValue() {
  const std::int64_t maxSafe = INT64_MAX / Amount::UNITS_PER_NODO;
  const Amount a = Amount::fromNodo(maxSafe);
  assert(a.rawUnits() == maxSafe * Amount::UNITS_PER_NODO);
  assert(a.isPositive());
}

void testFromNodoOverflowThrows() {
  const std::int64_t overflowValue = INT64_MAX / Amount::UNITS_PER_NODO + 1;
  bool threw = false;
  try {
    Amount::fromNodo(overflowValue);
  } catch (const std::overflow_error &) {
    threw = true;
  }
  assert(threw &&
         "fromNodo must throw std::overflow_error for overflow values");
}

void testFromNodoNegativeThrows() {
  bool threw = false;
  try {
    Amount::fromNodo(-1);
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  assert(threw && "fromNodo must throw for negative values");
}

void testFromRawUnitsValid() {
  const Amount a = Amount::fromRawUnits(42);
  assert(a.rawUnits() == 42);
}

void testAdditionOverflowThrows() {
  const Amount large = Amount::fromRawUnits(INT64_MAX);
  const Amount one = Amount::fromRawUnits(1);
  bool threw = false;
  try {
    large + one;
  } catch (const std::overflow_error &) {
    threw = true;
  }
  assert(threw && "Addition must throw on overflow");
}

void testSubtractionUnderflowThrows() {
  const Amount small = Amount::fromRawUnits(5);
  const Amount large = Amount::fromRawUnits(10);
  bool threw = false;
  try {
    small - large;
  } catch (const std::underflow_error &) {
    threw = true;
  }
  assert(threw && "Subtraction must throw when result would be negative");
}

} // namespace

int main() {
  testFromNodoZero();
  testFromNodoSmallValue();
  testFromNodoMaxSafeValue();
  testFromNodoOverflowThrows();
  testFromNodoNegativeThrows();
  testFromRawUnitsValid();
  testAdditionOverflowThrows();
  testSubtractionUnderflowThrows();
  return 0;
}
