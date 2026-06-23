#include "hashtable.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#define IS_TTY _isatty(_fileno(stdout))
#else
#include <unistd.h>
#define IS_TTY (isatty(fileno(stdout)) != 0)
#endif

const char *color_red() {
  return IS_TTY ? "\033[31m" : "";
}
const char *color_green() {
  return IS_TTY ? "\033[32m" : "";
}
const char *color_yellow() {
  return IS_TTY ? "\033[33m" : "";
}
const char *color_reset() {
  return IS_TTY ? "\033[0m" : "";
}

#define TEST_ASSERT(cond, msg)                                                                                         \
  do {                                                                                                                 \
    if (!(cond)) {                                                                                                     \
      std::cerr << color_red() << "  Error: " << msg << color_reset() << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
      return false;                                                                                                    \
    }                                                                                                                  \
  } while (0)

#define TEST_EQUAL(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_NOTNULL(ptr, msg) TEST_ASSERT((ptr) != nullptr, msg)
#define TEST_NULL(ptr, msg) TEST_ASSERT((ptr) == nullptr, msg)

template <template <typename, typename> class Strategy> bool test_insert_and_find() {
  OrderedHashTable<int, int, Strategy> table;
  table.insert(1, 100);
  table.insert(2, 200);
  int *val = table.find(1);
  TEST_NOTNULL(val, "Key 1 exists");
  TEST_EQUAL(*val, 100, "Value for key 1");
  TEST_NULL(table.find(3), "Key 3 missing");
  return true;
}

template <template <typename, typename> class Strategy> bool test_update_existing() {
  OrderedHashTable<int, int, Strategy> table;
  table.insert(1, 100);
  table.insert(1, 200);
  int *val = table.find(1);
  TEST_NOTNULL(val, "Key exists after update");
  TEST_EQUAL(*val, 200, "Value updated");
  return true;
}

template <template <typename, typename> class Strategy> bool test_erase() {
  OrderedHashTable<int, int, Strategy> table;
  table.insert(1, 100);
  TEST_ASSERT(table.erase(1), "Erase returns true");
  TEST_NULL(table.find(1), "Key removed");
  TEST_ASSERT(!table.erase(1), "Erase missing returns false");
  return true;
}

template <template <typename, typename> class Strategy> bool test_operator_bracket() {
  OrderedHashTable<int, int, Strategy> table;
  table[1] = 42;
  TEST_EQUAL(table[1], 42, "operator[] inserts default");
  table[1] = 99;
  TEST_EQUAL(table[1], 99, "operator[] updates existing");
  return true;
}

template <template <typename, typename> class Strategy> bool test_load_factor_and_rehash() {
  OrderedHashTable<int, int, Strategy> table;
  table.set_max_load_factor(0.5f);
  for (int i = 0; i < 100; ++i) table.insert(i, i);
  float lf = table.load_factor();
  TEST_ASSERT(lf <= 0.5f + 0.01f, "Load factor respected (≤0.5)");
  return true;
}

template <template <typename, typename> class Strategy> bool test_iteration() {
  OrderedHashTable<int, int, Strategy> table;
  for (int i = 0; i < 10; ++i) table.insert(i, i * 10);
  int count = 0, sum = 0;
  for (const auto &entry : table) {
    ++count;
    sum += entry.second;
  }
  TEST_EQUAL(count, 10, "Iterator visits all entries");
  int expected_sum = 10 * (0 + 9) / 2 * 10; // 450
  TEST_EQUAL(sum, expected_sum, "Sum of values correct");
  return true;
}

template <template <typename, typename> class Strategy> bool test_const_find() {
  OrderedHashTable<int, int, Strategy> table;
  table.insert(42, 100);
  const auto &const_table = table;
  const int *val = const_table.find(42);
  TEST_NOTNULL(val, "const find works");
  TEST_EQUAL(*val, 100, "Const value correct");
  return true;
}

template <template <typename, typename> class Strategy> bool test_string_keys() {
  OrderedHashTable<std::string, int, Strategy> table;
  table.insert("hello", 1);
  table.insert("world", 2);
  int *val = table.find("hello");
  TEST_NOTNULL(val, "String key found");
  TEST_EQUAL(*val, 1, "String value correct");
  return true;
}

template <template <typename, typename> class Strategy> bool test_large_insert_find() {
  OrderedHashTable<int, int, Strategy> table;
  const int N = 5000;
  for (int i = 0; i < N; ++i) table.insert(i, i * 2);
  TEST_EQUAL(table.size(), static_cast<size_t>(N), "Size after large insert");
  for (int i = 0; i < N; ++i) {
    int *v = table.find(i);
    TEST_NOTNULL(v, ("Key " + std::to_string(i) + " found").c_str());
    TEST_EQUAL(*v, i * 2, "Value correct");
  }
  return true;
}

template <template <typename, typename> class Strategy> bool test_order_preservation() {
  OrderedHashTable<int, int, Strategy> table;
  const int N = 100;
  for (int i = 0; i < N; ++i) table.insert(i, i);
  for (int i = 20; i < 40; ++i) table.erase(i);
  for (int i = 60; i < 80; ++i) table.erase(i);

  std::vector<int> keys;
  for (const auto &entry : table) {
    keys.push_back(entry.first);
  }
  std::vector<int> expected;
  for (int i = 0; i < 20; ++i) expected.push_back(i);
  for (int i = 40; i < 60; ++i) expected.push_back(i);
  for (int i = 80; i < 100; ++i) expected.push_back(i);

  TEST_EQUAL(keys.size(), expected.size(), "Same number of remaining elements");
  for (size_t i = 0; i < keys.size(); ++i) {
    TEST_EQUAL(keys[i], expected[i], ("Order at position " + std::to_string(i)).c_str());
  }
  return true;
}

template <template <typename, typename> class Strategy> bool test_shrink_to_fit() {
  OrderedHashTable<int, int, Strategy> table;
  const int N = 1000;
  for (int i = 0; i < N; ++i) table.insert(i, i);
  size_t capacity_before = table.capacity();
  for (int i = 0; i < N * 0.9; ++i) table.erase(i);
  table.shrink_to_fit();
  size_t capacity_after = table.capacity();
  TEST_ASSERT(capacity_after < capacity_before, "Capacity reduced after shrink");
  TEST_EQUAL(table.size(), static_cast<size_t>(N * 0.1), "Correct size after deletions");
  return true;
}

template <template <typename, typename> class Strategy> bool test_reserve() {
  OrderedHashTable<int, int, Strategy> table;
  const int N = 500;
  table.reserve(N);
  size_t cap_before = table.capacity();
  for (int i = 0; i < N; ++i) table.insert(i, i);
  TEST_EQUAL(table.capacity(), cap_before, "Capacity unchanged after reserve");
  return true;
}

template <template <typename, typename> class Strategy> bool test_clear() {
  OrderedHashTable<int, int, Strategy> table;
  for (int i = 0; i < 100; ++i) table.insert(i, i);
  table.clear();
  TEST_EQUAL(table.size(), 0, "Size 0 after clear");
  TEST_ASSERT(table.empty(), "Table empty");
  for (int i = 0; i < 100; ++i) {
    TEST_NULL(table.find(i), "No key found after clear");
  }
  return true;
}

template <template <typename, typename> class Strategy> bool test_emplace() {
  OrderedHashTable<int, std::pair<int, int>, Strategy> table;
  table.emplace(1, std::make_pair(10, 20));
  auto *val = table.find(1);
  TEST_NOTNULL(val, "Emplaced entry found");
  TEST_EQUAL(val->first, 10, "First of pair");
  TEST_EQUAL(val->second, 20, "Second of pair");
  return true;
}

template <template <typename, typename> class Strategy> bool test_move_semantics() {
  OrderedHashTable<std::string, std::string, Strategy> table;
  std::string key = "move_key";
  std::string value = "move_value";
  table.insert(std::move(key), std::move(value));
  const std::string *v = table.find("move_key");
  TEST_NOTNULL(v, "Moved key found");
  TEST_EQUAL(*v, "move_value", "Moved value correct");
  return true;
}

template <template <typename, typename> class Strategy> bool test_mixed_operations() {
  OrderedHashTable<int, int, Strategy> table;
  for (int i = 0; i < 200; ++i) table.insert(i, i);
  for (int i = 0; i < 100; ++i) table.insert(i, i + 1000);
  for (int i = 150; i < 200; ++i) table.erase(i);
  for (int i = 0; i < 100; ++i) {
    int *v = table.find(i);
    TEST_NOTNULL(v, "Updated key exists");
    TEST_EQUAL(*v, i + 1000, "Updated value");
  }
  for (int i = 100; i < 150; ++i) {
    int *v = table.find(i);
    TEST_NOTNULL(v, "Unchanged key exists");
    TEST_EQUAL(*v, i, "Original value");
  }
  for (int i = 150; i < 200; ++i) {
    TEST_NULL(table.find(i), "Deleted key absent");
  }
  return true;
}

template <template <typename, typename> class Strategy> bool test_iterator_after_delete() {
  OrderedHashTable<int, int, Strategy> table;
  for (int i = 0; i < 20; ++i) table.insert(i, i);
  for (int i = 0; i < 10; ++i) table.erase(i);
  int count = 0, sum = 0;
  for (auto it = table.begin(); it != table.end(); ++it) {
    ++count;
    sum += *it->second;
  }
  TEST_EQUAL(count, 10, "Iterator sees 10 remaining elements");
  TEST_EQUAL(sum, 10 * (10 + 19) / 2, "Sum correct");
  return true;
}

template <template <typename, typename> class Strategy> bool test_const_iteration() {
  OrderedHashTable<int, int, Strategy> table;
  for (int i = 0; i < 5; ++i) table.insert(i, i * 10);
  const auto &ctable = table;
  int count = 0;
  for (auto it = ctable.begin(); it != ctable.end(); ++it) {
    ++count;
  }
  TEST_EQUAL(count, 5, "Const iterator visits all");
  return true;
}

template <template <typename, typename> class Strategy> bool test_edge_load_factors() {
  OrderedHashTable<int, int, Strategy> table;
  table.set_max_load_factor(0.1f);
  for (int i = 0; i < 50; ++i) table.insert(i, i);
  float lf = table.load_factor();
  TEST_ASSERT(lf <= 0.1f + 0.01f, "Low load factor respected");

  table.clear();
  table.set_max_load_factor(0.9f);
  for (int i = 0; i < 100; ++i) table.insert(i, i);
  lf = table.load_factor();
  TEST_ASSERT(lf <= 0.9f + 0.01f, "High load factor respected");
  return true;
}

template <template <typename, typename> class Strategy> bool test_collision_handling() {
  OrderedHashTable<int, int, Strategy> table(16);
  size_t cap = table.capacity();
  for (int i = 0; i < 100; ++i) {
    table.insert(i * static_cast<int>(cap), i);
  }
  for (int i = 0; i < 100; ++i) {
    int *v = table.find(i * static_cast<int>(cap));
    TEST_NOTNULL(v, ("Collision key " + std::to_string(i)).c_str());
    TEST_EQUAL(*v, i, "Collision value correct");
  }
  return true;
}

template <template <typename, typename> class Strategy> bool run_all_tests_for_strategy(const char *strategy_name) {
  std::cout << color_yellow() << "=== Testing " << strategy_name << " ===\n" << color_reset();

  struct Test {
    const char *name;
    bool (*func)();
  };

  std::vector<Test> tests = {{"test_insert_and_find", test_insert_and_find<Strategy>},
                             {"test_update_existing", test_update_existing<Strategy>},
                             {"test_erase", test_erase<Strategy>},
                             {"test_operator_bracket", test_operator_bracket<Strategy>},
                             {"test_load_factor_and_rehash", test_load_factor_and_rehash<Strategy>},
                             {"test_iteration", test_iteration<Strategy>},
                             {"test_const_find", test_const_find<Strategy>},
                             {"test_string_keys", test_string_keys<Strategy>},
                             {"test_large_insert_find", test_large_insert_find<Strategy>},
                             {"test_order_preservation", test_order_preservation<Strategy>},
                             {"test_shrink_to_fit", test_shrink_to_fit<Strategy>},
                             {"test_reserve", test_reserve<Strategy>},
                             {"test_clear", test_clear<Strategy>},
                             {"test_emplace", test_emplace<Strategy>},
                             {"test_move_semantics", test_move_semantics<Strategy>},
                             {"test_mixed_operations", test_mixed_operations<Strategy>},
                             {"test_iterator_after_delete", test_iterator_after_delete<Strategy>},
                             {"test_const_iteration", test_const_iteration<Strategy>},
                             {"test_edge_load_factors", test_edge_load_factors<Strategy>},
                             {"test_collision_handling", test_collision_handling<Strategy>}};

  using clock = std::chrono::high_resolution_clock;
  auto strategy_start = clock::now();
  bool all_ok = true;

  for (const auto &t : tests) {
    std::cout << "  " << t.name << "... ";
    auto start = clock::now();
    bool pass = t.func();
    auto end = clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (pass) {
      std::cout << color_green() << "PASS" << color_reset() << " (" << duration << " µs)\n";
    } else {
      std::cout << color_red() << "FAIL" << color_reset() << " (" << duration << " µs)\n";
      all_ok = false;
    }
  }

  auto strategy_end = clock::now();
  auto strategy_duration = std::chrono::duration_cast<std::chrono::microseconds>(strategy_end - strategy_start).count();
  std::cout << "  " << color_yellow() << "Total " << strategy_name << " time: " << strategy_duration << " µs"
            << color_reset() << "\n\n";

  return all_ok;
}

int main() {
  auto total_start = std::chrono::high_resolution_clock::now();
  bool all_ok = true;
  all_ok &= run_all_tests_for_strategy<ChainingStrategy>("ChainingStrategy");
  all_ok &= run_all_tests_for_strategy<LinearStrategy>("LinearStrategy");
  all_ok &= run_all_tests_for_strategy<QuadraticStrategy>("QuadraticStrategy");

  auto total_end = std::chrono::high_resolution_clock::now();
  auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start).count();

  std::cout << color_yellow() << "\n=== Overall result ===\n" << color_reset();
  if (all_ok)
    std::cout << color_green() << "ALL TESTS PASSED" << color_reset();
  else
    std::cout << color_red() << "SOME TESTS FAILED" << color_reset();
  std::cout << " (total time: " << total_duration << " µs)\n";

  return all_ok ? 0 : 1;
}
