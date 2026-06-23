#pragma once
#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include <cstddef>
#include <forward_list>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#if __cplusplus >= 202002L
#define NODISCARD(reason) [[nodiscard(reason)]]
#elif __cplusplus >= 201703L
#define NODISCARD(reason) [[nodiscard]]
#else
#define NODISCARD(reason)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

/**
 * @brief Default hash function for arbitrary type using std::hash.
 * @tparam T Key type.
 */
template <typename T> struct Hash {
  NODISCARD("Hash value is used for table indexing")
  size_t operator()(const T &key) const { return std::hash<T>{}(key); }
};

/**
 * @brief Hash specialization for std::string using DJB2 algorithm.
 */
template <> struct Hash<std::string> {
  static constexpr size_t DJB2_START = 5381;

  NODISCARD("Hash value is used for table indexing")
  size_t operator()(const std::string &str) const {
    size_t hash = DJB2_START;
    for (char ch : str) {
      hash = ((hash << 5) + hash) + static_cast<size_t>(ch);
    }
    return hash;
  }
};

/**
 * @brief Stores key, value, and flags for open addressing.
 * @tparam Key  Key type.
 * @tparam Value Value type.
 */
template <typename Key, typename Value> struct Entry {
  Key key{};
  Value value{};
  bool active = true;
  bool deleted = false; // Can be compacted during rehash
  Entry() = default;
  Entry(const Key &k, const Value &v) : key(k), value(v) {}
  Entry(Key &&k, Value &&v) : key(std::move(k)), value(std::move(v)) {}
};

#if __cplusplus >= 202002L
#include <concepts>
template <typename K, typename Hash>
concept Hashable = requires(const K &key, Hash hash) {
  { hash(key) } -> std::convertible_to<size_t>;
};
template <typename Key, typename Hash, typename Value, template <typename, typename> class Strategy>
concept CollisionStrategyConcept = requires(Strategy<Key, Hash> strategy, const Key &key, size_t idx,
                                            const std::vector<Entry<Key, Value>> &entries, const Hash &hash) {
  { strategy.insert_or_find(key, idx, entries, hash) } -> std::same_as<std::pair<bool, size_t>>;
  { strategy.find(key, entries, hash) } -> std::convertible_to<size_t>;
  { strategy.erase(key, entries, hash) } -> std::convertible_to<bool>;
  { strategy.rehash(size_t{}, entries, hash) } -> std::same_as<void>;
  { strategy.clear() } -> std::same_as<void>;
  { strategy.capacity() } -> std::convertible_to<size_t>;
  { strategy.size() } -> std::convertible_to<size_t>;
};
#endif

// Helper to round up to the next power of two (used by both strategies)
namespace detail {
inline size_t next_power_of_two(size_t n) {
  if (n <= 2) return 2;
  size_t power = 2;
  while (power < n) power <<= 1;
  return power;
}
} // namespace detail

/**
 * @brief Collision resolution strategy using chaining (separate lists).
 * @tparam Key  Key type.
 * @tparam Hash Hash function type.
 */
template <typename Key, typename Hash = ::Hash<Key>> class ChainingStrategy {
private:
  static constexpr size_t DEFAULT_INITIAL_CAPACITY = 8;
  static constexpr size_t GROWTH_FACTOR = 2;
  static constexpr size_t MIN_CAPACITY = 8;

  std::vector<std::forward_list<std::pair<Key, size_t>>> buckets;
  size_t num_elements = 0;      // current number of elements
  float max_load_factor = 1.0f; // maximum load factor
  size_t max_elements = 0;      // maximum number of elements before rehash

  void update_max_elements() noexcept {
    max_elements = static_cast<size_t>(static_cast<float>(buckets.size()) * max_load_factor);
    if (max_elements < 1) max_elements = 1;
  }

public:
  static constexpr float DEFAULT_MAX_LOAD = 1.0f;
  enum { NOT_FOUND = std::numeric_limits<size_t>::max() };

  /**
   * @brief Returns the strategy name for use in reports.
   */
  static const char *name() { return "Chaining"; }

  explicit ChainingStrategy(size_t initial_capacity = DEFAULT_INITIAL_CAPACITY, float ml = DEFAULT_MAX_LOAD)
      : max_load_factor(ml > 0.0f ? ml : DEFAULT_MAX_LOAD) {
    initial_capacity = detail::next_power_of_two(initial_capacity);
    if (initial_capacity < MIN_CAPACITY) initial_capacity = MIN_CAPACITY;
    buckets.resize(initial_capacity);
    update_max_elements();
  }

  void set_max_load_factor(float ml) noexcept {
    if (ml <= 0.0f) return;
    max_load_factor = ml;
    update_max_elements();
  }

  float get_max_load_factor() const noexcept { return max_load_factor; }

  /**
   * @brief Attempts to insert a key or find an existing one.
   * @param key       The key.
   * @param new_index Index to be assigned to the new entry in the entries array.
   * @param entries   Vector of entries (not used in chaining, but required for interface).
   * @param hash      Hash function.
   * @return pair: first == true if inserted new element, false if existing found.
   *         second – index of the entry in the entries array (new or existing).
   */
  template <typename Value>
  std::pair<bool, size_t> insert_or_find(const Key &key, size_t new_index,
                                         const std::vector<Entry<Key, Value>> & /*entries*/, const Hash &hash) {
    const size_t mask = buckets.size() - 1;
    const size_t bucket_index = hash(key) & mask;
    auto &bucket = buckets[bucket_index];
    for (auto &pair : bucket) {
      if (pair.first == key) {
        return {false, pair.second};
      }
    }
    bucket.emplace_front(key, new_index);
    ++num_elements;
    if (UNLIKELY(num_elements > max_elements)) {
      rehash(buckets.size() * GROWTH_FACTOR, std::vector<Entry<Key, Value>>{}, hash);
    }
    return {true, new_index};
  }

  /**
   * @brief Finds the index for a given key.
   * @return Index of the entry or NOT_FOUND.
   */
  template <typename Value>
  NODISCARD("Returned index must be checked against NOT_FOUND")
  size_t find(const Key &key, const std::vector<Entry<Key, Value>> & /*entries*/, const Hash &hash) const {
    const size_t mask = buckets.size() - 1;
    const size_t bucket_index = hash(key) & mask;
    for (const auto &pair : buckets[bucket_index]) {
      if (pair.first == key) return pair.second;
    }
    return NOT_FOUND;
  }

  /**
   * @brief Erases an element by key.
   * @return true if the element was found and removed.
   */
  template <typename Value>
  bool erase(const Key &key, const std::vector<Entry<Key, Value>> & /*entries*/, const Hash &hash) {
    const size_t mask = buckets.size() - 1;
    const size_t bucket_index = hash(key) & mask;
    auto &bucket = buckets[bucket_index];
    auto prev = bucket.before_begin();
    auto it = bucket.begin();
    while (it != bucket.end()) {
      if (it->first == key) {
        bucket.erase_after(prev);
        --num_elements;
        return true;
      }
      prev = it;
      ++it;
    }
    return false;
  }

  /**
   * @brief Rehashes with a new number of buckets (rounded to next power of two).
   */
  template <typename Value>
  void rehash(size_t new_size, const std::vector<Entry<Key, Value>> & /*entries*/, const Hash &hash) {
    new_size = detail::next_power_of_two(new_size);
    if (new_size < MIN_CAPACITY) new_size = MIN_CAPACITY;
    std::vector<std::forward_list<std::pair<Key, size_t>>> new_buckets(new_size);
    const size_t new_mask = new_size - 1;
    for (const auto &bucket : buckets) {
      for (const auto &pair : bucket) {
        const size_t idx = hash(pair.first) & new_mask;
        new_buckets[idx].emplace_front(pair.first, pair.second);
      }
    }
    buckets.swap(new_buckets);
    update_max_elements();
  }

  void clear() noexcept {
    for (auto &bucket : buckets) bucket.clear();
    num_elements = 0;
  }

  NODISCARD("Table capacity needed for rehashing decisions")
  size_t capacity() const noexcept { return buckets.size(); }

  NODISCARD("Element count needed for load factor checks")
  size_t size() const noexcept { return num_elements; }
};

enum class ProbingStrategy { Linear, Quadratic };

/**
 * @brief Helper template to compute the probing step.
 */
template <ProbingStrategy Strategy> struct Probe {
  static size_t increment(size_t /*step*/) { return 1; }
};

template <> struct Probe<ProbingStrategy::Quadratic> {
  static size_t increment(size_t step) { return step + 1; }
};

/**
 * @brief Open addressing strategy with a given probing method.
 * @tparam Key      Key type.
 * @tparam Hash     Hash function.
 * @tparam Strategy Probing strategy (Linear or Quadratic).
 */
template <typename Key, typename Hash = ::Hash<Key>, ProbingStrategy Strategy = ProbingStrategy::Linear>
class OpenAddressingStrategy {
private:
  static constexpr size_t DEFAULT_INITIAL_CAPACITY = 8;
  static constexpr size_t GROWTH_FACTOR = 2;
  static constexpr size_t MIN_CAPACITY = 8;

  enum : size_t {
    EMPTY_SLOT = std::numeric_limits<size_t>::max(),
    DELETED_SLOT = std::numeric_limits<size_t>::max() - 1
  };

  std::vector<size_t> slots; // indices of entries or special markers
  size_t num_elements = 0;
  float max_load_factor = 0.5f;
  size_t max_elements = 0;

  void update_max_elements() noexcept {
    max_elements = static_cast<size_t>(static_cast<float>(slots.size()) * max_load_factor);
    if (max_elements < 1) max_elements = 1;
  }

public:
  static constexpr float DEFAULT_MAX_LOAD = 0.5f;
  enum { NOT_FOUND = std::numeric_limits<size_t>::max() };

  /**
   * @brief Returns the strategy name depending on the probing method.
   */
  static const char *name() {
    if constexpr (Strategy == ProbingStrategy::Linear)
      return "Linear";
    else
      return "Quadratic";
  }

  explicit OpenAddressingStrategy(size_t initial_capacity = DEFAULT_INITIAL_CAPACITY, float ml = DEFAULT_MAX_LOAD)
      : max_load_factor((ml > 0.0f) ? (ml > 1.0f ? 1.0f : ml) : DEFAULT_MAX_LOAD) {
    initial_capacity = detail::next_power_of_two(initial_capacity);
    if (initial_capacity < MIN_CAPACITY) initial_capacity = MIN_CAPACITY;
    slots.assign(initial_capacity, EMPTY_SLOT);
    update_max_elements();
  }

  void set_max_load_factor(float ml) noexcept {
    if (ml <= 0.0f) return;
    max_load_factor = (ml > 1.0f) ? 1.0f : ml;
    update_max_elements();
  }

  float get_max_load_factor() const noexcept { return max_load_factor; }

  /**
   * @brief Inserts a key or finds an existing one.
   * @details Uses probing according to the strategy.
   * @param key       The key.
   * @param new_index Index of the new entry (if insertion).
   * @param entries   Vector of all entries (for key comparison).
   * @param hash      Hash function.
   * @return pair: first == true if new element inserted, false if existing updated.
   *         second – index of the entry in the entries array.
   */
  template <typename Value>
  std::pair<bool, size_t> insert_or_find(const Key &key, size_t new_index,
                                         const std::vector<Entry<Key, Value>> &entries, const Hash &hash) {
    while (true) {
      if (UNLIKELY(num_elements >= max_elements)) {
        rehash(slots.size() * GROWTH_FACTOR, entries, hash);
      }

      const size_t mask = slots.size() - 1;
      size_t index = hash(key) & mask;
      size_t step = 0;

      while (step < slots.size()) {
        const size_t slot_index = slots[index];
        if (slot_index == EMPTY_SLOT || slot_index == DELETED_SLOT) {
          slots[index] = new_index;
          ++num_elements;
          return {true, new_index};
        } else {
          const auto &entry = entries[slot_index];
          if (entry.active && !entry.deleted && entry.key == key) {
            return {false, slot_index};
          }
        }
        ++step;
        index = (index + Probe<Strategy>::increment(step)) & mask;
      }

      // Table is completely full (should not happen because we control the load)
      rehash(slots.size() * GROWTH_FACTOR, entries, hash);
    }
  }

  /**
   * @brief Finds the entry index for a given key.
   * @return Index of the entry or NOT_FOUND.
   */
  template <typename Value>
  NODISCARD("Returned index must be checked against NOT_FOUND")
  size_t find(const Key &key, const std::vector<Entry<Key, Value>> &entries, const Hash &hash) const {
    if (slots.empty()) return NOT_FOUND;
    const size_t mask = slots.size() - 1;
    size_t index = hash(key) & mask;
    size_t step = 0;
    while (step < slots.size()) {
      const size_t slot_index = slots[index];
      if (slot_index == EMPTY_SLOT) return NOT_FOUND;
      if (slot_index != DELETED_SLOT) {
        const auto &entry = entries[slot_index];
        if (entry.active && !entry.deleted && entry.key == key) return slot_index;
      }
      ++step;
      index = (index + Probe<Strategy>::increment(step)) & mask;
    }
    return NOT_FOUND;
  }

  /**
   * @brief Erases an element by key.
   * @return true if the element was found and marked as deleted.
   */
  template <typename Value>
  bool erase(const Key &key, const std::vector<Entry<Key, Value>> &entries, const Hash &hash) {
    if (slots.empty()) return false;
    const size_t mask = slots.size() - 1;
    size_t index = hash(key) & mask;
    size_t step = 0;
    while (step < slots.size()) {
      const size_t slot_index = slots[index];
      if (slot_index == EMPTY_SLOT) return false;
      if (slot_index != DELETED_SLOT) {
        const auto &entry = entries[slot_index];
        if (entry.active && !entry.deleted && entry.key == key) {
          slots[index] = DELETED_SLOT;
          --num_elements;
          return true;
        }
      }
      ++step;
      index = (index + Probe<Strategy>::increment(step)) & mask;
    }
    return false;
  }

  /**
   * @brief Rehashes with a new table size (rounded to next power of two).
   */
  template <typename Value>
  void rehash(size_t new_size, const std::vector<Entry<Key, Value>> &entries, const Hash &hash) {
    new_size = detail::next_power_of_two(new_size);
    if (new_size < MIN_CAPACITY) new_size = MIN_CAPACITY;
    std::vector<size_t> new_slots(new_size, EMPTY_SLOT);
    const size_t mask = new_size - 1;
    for (size_t idx = 0; idx < entries.size(); ++idx) {
      const auto &entry = entries[idx];
      if (entry.active && !entry.deleted) {
        size_t index = hash(entry.key) & mask;
        size_t step = 0;
        while (new_slots[index] != EMPTY_SLOT) {
          ++step;
          index = (index + Probe<Strategy>::increment(step)) & mask;
        }
        new_slots[index] = idx;
      }
    }
    slots.swap(new_slots);
    update_max_elements();
  }

  void clear() noexcept {
    slots.assign(DEFAULT_INITIAL_CAPACITY, EMPTY_SLOT);
    num_elements = 0;
    update_max_elements();
  }

  NODISCARD("Table capacity needed for rehashing decisions")
  size_t capacity() const noexcept { return slots.size(); }

  NODISCARD("Element count needed for load factor checks")
  size_t size() const noexcept { return num_elements; }
};

// For convenience
template <typename Key, typename Hash = ::Hash<Key>>
using LinearStrategy = OpenAddressingStrategy<Key, Hash, ProbingStrategy::Linear>;

template <typename Key, typename Hash = ::Hash<Key>>
using QuadraticStrategy = OpenAddressingStrategy<Key, Hash, ProbingStrategy::Quadratic>;

/**
 * @brief Ordered hash table with configurable collision resolution strategy.
 * @tparam Key            Key type.
 * @tparam Value          Value type.
 * @tparam CollisionStrategy Collision resolution strategy (default QuadraticStrategy).
 * @tparam Hash           Hash function (default ::Hash<Key>).
 */
template <typename Key, typename Value, template <typename, typename> class CollisionStrategy = QuadraticStrategy,
          typename Hash = ::Hash<Key>>
#if __cplusplus >= 202002L
  requires(Hashable<Key, Hash> && CollisionStrategyConcept<Key, Hash, Value, CollisionStrategy>)
#endif
class OrderedHashTable {
private:
  using Entry = ::Entry<Key, Value>;
  using Strategy = CollisionStrategy<Key, Hash>;

  float max_load_factor;
  Strategy strategy;
  Hash hash;
  std::vector<Entry> entries; // all entries in insertion order
  size_t active_count = 0;    // number of active (not deleted) entries

  static constexpr size_t DEFAULT_INITIAL_CAPACITY = 8;
  static constexpr size_t GROWTH_FACTOR = 2;
  static constexpr size_t SHRINK_THRESHOLD = 4;

  /**
   * @brief Minimum capacity required to store count elements (rounded up to power of two).
   */
  size_t min_capacity_for(size_t count) const {
    if (max_load_factor <= 0.0f) return detail::next_power_of_two(count);
    size_t needed = static_cast<size_t>(static_cast<float>(count) / max_load_factor) + 1;
    return detail::next_power_of_two(needed);
  }

  bool should_shrink() const {
    return static_cast<float>(active_count * SHRINK_THRESHOLD) <
           static_cast<float>(strategy.capacity()) * max_load_factor;
  }

  /**
   * @brief Rebuilds the table, compressing the entries array and updating indices.
   */
  void compress_and_rebuild(size_t new_capacity) {
    std::vector<Entry> new_entries;
    new_entries.reserve(active_count);
    for (auto &entry : entries) {
      if (entry.active && !entry.deleted) {
        new_entries.push_back(std::move(entry));
      }
    }
    size_t min_cap = min_capacity_for(active_count);
    if (new_capacity < min_cap) new_capacity = min_cap;
    strategy = Strategy(new_capacity, max_load_factor);
    entries.swap(new_entries);
    for (size_t idx = 0; idx < entries.size(); ++idx) {
      (void)strategy.insert_or_find(entries[idx].key, idx, entries, hash);
    }
  }

  /**
   * @brief Inserts or updates an entry, returning a reference to the value.
   */
  Value &insert_or_assign_entry(Entry &&entry) {
    size_t existing = strategy.find(entry.key, entries, hash);
    if (existing != Strategy::NOT_FOUND) {
      entries[existing].value = std::move(entry.value);
      entries[existing].active = true;
      entries[existing].deleted = false;
      return entries[existing].value;
    }
    size_t new_index = entries.size();
    auto result = strategy.insert_or_find(entry.key, new_index, entries, hash);
    if (LIKELY(result.first)) {
      entries.push_back(std::move(entry));
      ++active_count;
      return entries.back().value;
    } else {
      entries[result.second].value = std::move(entry.value);
      entries[result.second].active = true;
      entries[result.second].deleted = false;
      return entries[result.second].value;
    }
  }

public:
  explicit OrderedHashTable(size_t initial_capacity = DEFAULT_INITIAL_CAPACITY)
      : max_load_factor(Strategy::DEFAULT_MAX_LOAD), strategy(initial_capacity, max_load_factor), hash(), entries() {
    entries.reserve(initial_capacity);
  }

  /**
   * @brief Sets the maximum load factor.
   */
  void set_max_load_factor(float mlf) {
    if (mlf <= 0.0f) return;
    max_load_factor = mlf;
    strategy.set_max_load_factor(mlf);
    if (active_count > 0) {
      compress_and_rebuild(strategy.capacity());
    }
  }

  float get_max_load_factor() const { return max_load_factor; }

  /**
   * @brief Current load factor.
   */
  float load_factor() const {
    if (strategy.capacity() == 0) return 0.0f;
    return static_cast<float>(active_count) / static_cast<float>(strategy.capacity());
  }

  /**
   * @brief Reserves memory to store at least count elements.
   */
  void reserve(size_t count) {
    entries.reserve(count);
    size_t min_cap = min_capacity_for(count);
    if (min_cap > strategy.capacity()) {
      compress_and_rebuild(min_cap);
    }
  }

  /**
   * @brief Shrinks the table to the minimum possible size.
   */
  void shrink_to_fit() {
    if (should_shrink()) {
      size_t new_capacity = min_capacity_for(active_count);
      if (new_capacity < DEFAULT_INITIAL_CAPACITY) new_capacity = DEFAULT_INITIAL_CAPACITY;
      compress_and_rebuild(new_capacity);
      entries.shrink_to_fit();
    }
  }

  /**
   * @brief Inserts or updates a value by key (copy).
   */
  void insert(const Key &key, const Value &value) { insert_or_assign_entry(Entry(key, value)); }

  /**
   * @brief Inserts or updates a value by key (move).
   */
  void insert(Key &&key, Value &&value) { insert_or_assign_entry(Entry(std::move(key), std::move(value))); }

  /**
   * @brief Constructs and inserts an element in place.
   */
  template <typename... Args> void emplace(Args &&...args) {
    insert_or_assign_entry(Entry(std::forward<Args>(args)...));
  }

  /**
   * @brief Finds a value by key (non-const version).
   * @return Pointer to the value or nullptr if not found.
   */
  NODISCARD("Returned pointer may be null; check before dereferencing")
  Value *find(const Key &key) {
    size_t index = strategy.find(key, entries, hash);
    if (UNLIKELY(index == Strategy::NOT_FOUND)) return nullptr;
    return &entries[index].value;
  }

  /**
   * @brief Finds a value by key (const version).
   * @return Pointer to the value or nullptr.
   */
  NODISCARD("Returned pointer may be null; check before dereferencing")
  const Value *find(const Key &key) const {
    size_t index = strategy.find(key, entries, hash);
    if (UNLIKELY(index == Strategy::NOT_FOUND)) return nullptr;
    return &entries[index].value;
  }

  /**
   * @brief Erases an element by key.
   * @return true if the element was found and removed.
   */
  bool erase(const Key &key) {
    size_t index = strategy.find(key, entries, hash);
    if (UNLIKELY(index == Strategy::NOT_FOUND)) return false;
    if (strategy.erase(key, entries, hash)) {
      entries[index].active = false;
      entries[index].deleted = true;
      --active_count;
      if (UNLIKELY(should_shrink())) shrink_to_fit();
      return true;
    }
    return false;
  }

  /**
   * @brief Subscript operator (default-inserts value if key missing).
   */
  Value &operator[](const Key &key) {
    size_t index = strategy.find(key, entries, hash);
    if (LIKELY(index != Strategy::NOT_FOUND)) return entries[index].value;
    return insert_or_assign_entry(Entry(key, Value{}));
  }

  bool empty() const noexcept { return active_count == 0; }
  size_t size() const noexcept { return active_count; }
  size_t capacity() const noexcept { return strategy.capacity(); }

  void clear() noexcept {
    entries.clear();
    strategy.clear();
    active_count = 0;
  }

  class iterator {
    std::vector<Entry> *entries_ptr;
    size_t position;

  public:
    iterator(std::vector<Entry> *ptr, size_t start_pos) : entries_ptr(ptr), position(start_pos) {}

    iterator &operator++() {
      ++position;
      while (position < entries_ptr->size() && (!(*entries_ptr)[position].active || (*entries_ptr)[position].deleted)) {
        ++position;
      }
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const iterator &other) const {
      return position == other.position && entries_ptr == other.entries_ptr;
    }
    bool operator!=(const iterator &other) const { return !(*this == other); }

    std::pair<const Key &, Value &> operator*() const {
      Entry &entry = (*entries_ptr)[position];
      return std::pair<const Key &, Value &>(entry.key, entry.value);
    }

    struct ArrowProxy {
      std::pair<const Key *, Value *> key_value_pointers;
      ArrowProxy(const Key &key, Value &value) : key_value_pointers(&key, &value) {}
      std::pair<const Key &, Value &> operator*() const {
        return {*key_value_pointers.first, *key_value_pointers.second};
      }
      const std::pair<const Key *, Value *> *operator->() const { return &key_value_pointers; }
    };

    ArrowProxy operator->() const {
      Entry &entry = (*entries_ptr)[position];
      return ArrowProxy(entry.key, entry.value);
    }
  };

  class const_iterator {
    const std::vector<Entry> *entries_ptr;
    size_t position;

  public:
    const_iterator(const std::vector<Entry> *ptr, size_t start_pos) : entries_ptr(ptr), position(start_pos) {}

    const_iterator &operator++() {
      ++position;
      while (position < entries_ptr->size() && (!(*entries_ptr)[position].active || (*entries_ptr)[position].deleted)) {
        ++position;
      }
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const const_iterator &other) const {
      return position == other.position && entries_ptr == other.entries_ptr;
    }
    bool operator!=(const const_iterator &other) const { return !(*this == other); }

    std::pair<const Key &, const Value &> operator*() const {
      const Entry &entry = (*entries_ptr)[position];
      return std::pair<const Key &, const Value &>(entry.key, entry.value);
    }

    struct ArrowProxy {
      std::pair<const Key *, const Value *> pair;
      ArrowProxy(const Key &key, const Value &value) : pair(&key, &value) {}
      std::pair<const Key &, const Value &> operator*() const { return {*pair.first, *pair.second}; }
      const std::pair<const Key *, const Value *> *operator->() const { return &pair; }
    };

    ArrowProxy operator->() const {
      const Entry &entry = (*entries_ptr)[position];
      return ArrowProxy(entry.key, entry.value);
    }
  };

  iterator begin() noexcept {
    size_t pos = 0;
    while (pos < entries.size() && (!entries[pos].active || entries[pos].deleted)) ++pos;
    return iterator(&entries, pos);
  }

  const_iterator begin() const noexcept {
    size_t pos = 0;
    while (pos < entries.size() && (!entries[pos].active || entries[pos].deleted)) ++pos;
    return const_iterator(&entries, pos);
  }

  iterator end() noexcept { return iterator(&entries, entries.size()); }

  const_iterator end() const noexcept { return const_iterator(&entries, entries.size()); }
};

#endif // HASHTABLE_HPP
