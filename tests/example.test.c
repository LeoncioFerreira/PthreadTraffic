#include "unity.h"
#include <stdbool.h>
#include <stdio.h>
volatile bool keep_running = true;

void setUp() {}
void tearDown() {}

void test_simple_pass() { TEST_ASSERT_EQUAL_INT(1, 1); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple_pass);
  return UNITY_END();
}
