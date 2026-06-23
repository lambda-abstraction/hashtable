#include "hashtable.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

enum class Distribution { Sequential, Uniform, Bad };

struct Stats {
  double mean, stddev, min, max;
  size_t n;
};

Stats compute_stats(const std::vector<long long> &times) {
  if (times.empty()) return {0.0, 0.0, 0.0, 0.0, 0};
  double sum = 0.0;
  for (auto t : times) sum += static_cast<double>(t);
  double mean = sum / static_cast<double>(times.size());
  double sq = 0.0;
  for (auto t : times) {
    double d = static_cast<double>(t) - mean;
    sq += d * d;
  }
  double stddev = std::sqrt(sq / static_cast<double>(times.size()));
  auto mm = std::minmax_element(times.begin(), times.end());
  return {mean, stddev, static_cast<double>(*mm.first), static_cast<double>(*mm.second), times.size()};
}

/**
 * @brief Measures the execution time of a function multiple times, returning measurements in nanoseconds.
 * @param func       Function to measure.
 * @param warmup     Number of warm-up runs.
 * @param repetitions Number of measurement repetitions.
 * @return Vector of times in nanoseconds.
 */
template <typename Func> std::vector<long long> measure_times(Func &&func, int warmup = 15, size_t repetitions = 100) {
  for (int i = 0; i < warmup; ++i) func();
  std::vector<long long> times;
  times.reserve(repetitions);
  for (size_t i = 0; i < repetitions; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
  }
  return times;
}

/**
 * @brief Generates a set of keys according to the specified distribution.
 * @param N        Number of keys.
 * @param distrib  Distribution type.
 * @return Vector of keys.
 */
std::vector<int> generate_keys(size_t N, Distribution distrib) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::vector<int> keys;
  keys.reserve(N);

  switch (distrib) {
  case Distribution::Sequential:
    for (size_t i = 0; i < N; ++i) keys.push_back(static_cast<int>(i));
    break;
  case Distribution::Uniform: {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(N * 2));
    for (size_t i = 0; i < N; ++i) keys.push_back(dist(gen));
    break;
  }
  case Distribution::Bad:
    for (size_t i = 0; i < N; ++i) keys.push_back(static_cast<int>(i * 1024));
    break;
  }
  return keys;
}

/**
 * @brief Runs the benchmark for a given strategy and writes results to CSV.
 * @tparam Strategy  Collision resolution strategy class.
 * @param N           Number of elements to insert.
 * @param lookups     Number of lookup operations.
 * @param distrib     Key distribution.
 * @param load_factor Load factor (-1 for default).
 * @param out         Output CSV stream.
 */
template <template <typename, typename> class Strategy>
void benchmark_strategy(size_t N, size_t lookups, Distribution distrib,
                        float load_factor, // -1 for default
                        std::ostream &out) {
  using Table = OrderedHashTable<int, int, Strategy>;
  const std::string name = Strategy<int, Hash<int>>::name();

  const auto keys = generate_keys(N, distrib);

  auto insert_func = [&]() {
    Table table;
    if (load_factor > 0) table.set_max_load_factor(load_factor);
    for (int k : keys) table.insert(k, k * k);
  };
  auto insert_times = measure_times(insert_func);
  Stats ins = compute_stats(insert_times);

  ins.mean /= static_cast<double>(N);
  ins.stddev /= static_cast<double>(N);
  ins.min /= static_cast<double>(N);
  ins.max /= static_cast<double>(N);

  Table table;
  if (load_factor > 0) table.set_max_load_factor(load_factor);
  for (int k : keys) table.insert(k, k * k);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::vector<int> lookup_keys;
  lookup_keys.reserve(lookups);
  std::uniform_int_distribution<int> exist(0, static_cast<int>(N) - 1);
  std::uniform_int_distribution<int> missing(static_cast<int>(N), static_cast<int>(N * 2));
  for (size_t i = 0; i < lookups; ++i) {
    if (gen() % 2 == 0) {
      int idx = exist(gen);
      lookup_keys.push_back(keys[static_cast<size_t>(idx)]);
    } else {
      lookup_keys.push_back(missing(gen));
    }
  }

  volatile int sink = 0;
  auto lookup_func = [&]() {
    for (int k : lookup_keys) {
      int *val = table.find(k);
      if (val) sink ^= *val;
    }
  };
  auto lookup_times = measure_times(lookup_func);
  Stats lkp = compute_stats(lookup_times);
  lkp.mean /= static_cast<double>(lookups);
  lkp.stddev /= static_cast<double>(lookups);
  lkp.min /= static_cast<double>(lookups);
  lkp.max /= static_cast<double>(lookups);

  const char *distrib_str = (distrib == Distribution::Sequential) ? "sequential"
                          : (distrib == Distribution::Uniform)    ? "uniform"
                                                                  : "bad";

  auto write_row = [&](const char *op, const Stats &st) {
    if (load_factor < 0) {
      out << name << "," << N << "," << distrib_str << "," << op << "," << st.mean << "," << st.stddev << "," << st.min
          << "," << st.max << "," << st.n << "\n";
    } else {
      out << name << "," << N << "," << distrib_str << "," << load_factor << "," << op << "," << st.mean << ","
          << st.stddev << "," << st.min << "," << st.max << "," << st.n << "\n";
    }
  };

  write_row("Insert", ins);
  write_row("Lookup", lkp);
}

int main(int argc, char *argv[]) {
  const char *bench_csv_path = "benchmark.csv";
  const char *load_csv_path = "load_benchmark.csv";

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--bench-csv") == 0 && i + 1 < argc) {
      bench_csv_path = argv[++i];
    } else if (strcmp(argv[i], "--load-csv") == 0 && i + 1 < argc) {
      load_csv_path = argv[++i];
    } else {
      std::cerr << "Usage: " << argv[0] << " [--bench-csv <file>] [--load-csv <file>]\n";
      return 1;
    }
  }

  std::ofstream bench_csv(bench_csv_path);
  std::ofstream load_csv(load_csv_path);
  if (!bench_csv.is_open() || !load_csv.is_open()) {
    std::cerr << "Error opening output files.\n";
    return 1;
  }

  const std::vector<size_t> sizes = {1000,  2500,  3000,  4000,  5000,  6000,  7500,  10000, 15000,
                                     20000, 25000, 35000, 40000, 50000, 60000, 75000, 100000};
  const size_t lookups = 10000;
  const std::vector<Distribution> distribs = {Distribution::Sequential, Distribution::Uniform, Distribution::Bad};

  const std::vector<float> load_factors_open = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 0.95f};
  const std::vector<float> load_factors_chaining = {0.1f, 0.2f,  0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
                                                    0.9f, 0.95f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};

  bench_csv << "Strategy,N,Distribution,Operation,mean,stddev,min,max,samples\n";
  load_csv << "Strategy,N,Distribution,LoadFactor,Operation,mean,stddev,min,max,samples\n";

  for (size_t N : sizes) {
    for (auto d : distribs) {
      benchmark_strategy<ChainingStrategy>(N, lookups, d, -1, bench_csv);
      benchmark_strategy<LinearStrategy>(N, lookups, d, -1, bench_csv);
      benchmark_strategy<QuadraticStrategy>(N, lookups, d, -1, bench_csv);
    }
  }

  const size_t LOAD_N = 10000;
  for (auto d : distribs) {
    for (auto lf : load_factors_chaining) {
      benchmark_strategy<ChainingStrategy>(LOAD_N, lookups, d, lf, load_csv);
    }
    for (auto lf : load_factors_open) {
      benchmark_strategy<LinearStrategy>(LOAD_N, lookups, d, lf, load_csv);
      benchmark_strategy<QuadraticStrategy>(LOAD_N, lookups, d, lf, load_csv);
    }
  }

  std::cout << "Benchmark completed.\n";
  std::cout << "  - " << bench_csv_path << " (standard)\n";
  std::cout << "  - " << load_csv_path << " (load factor)\n";
  return 0;
}
