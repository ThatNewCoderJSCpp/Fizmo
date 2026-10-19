# Trie, TrieMap and TrieSet

`#include <collections.hpp>` (or `FIZMO_COLLECTIONS` with `fizmo/includes.hpp`) gives a trie: a tree of letters where every word that starts the same way shares the same branch. It's the container for autocomplete, "did you mean", prefix search and longest-match tokenizing.

| Type | For |
|---|---|
| `fizmo::TrieMap<V>` | a map from strings to `V`, searchable by prefix |
| `fizmo::TrieSet<>` | a set of strings, searchable by prefix |
| `fizmo::Trie` | the same as `TrieSet<>` |

All three live in `fizmo::collections` too. Keys are bytes, so UTF-8 works as it is; words come back in byte order, which for ASCII is alphabetical.

```cpp
fizmo::TrieMap<int> words;
words.insert("vector", 1);
words.insert("vec", 2);
words["value"] = 3;

words.keys_with_prefix("ve");        // {"vec", "vector"}
words.count_prefix("v");             // 3
words.complete("vo");                // "vo" (nothing starts with "vo")
words.complete("vect");              // "vector": the only way to go on
words.longest_prefix_of("values!");  // {5, &3}: "value" is the longest key at the start
words.erase("vec");
```

## Methods

| Method | Does |
|---|---|
| `insert(key, value)` | adds `key` if it isn't there; `true` when it was added |
| `insert_or_assign(key, value)` | adds or replaces; `true` when it was added |
| `map[key]` | the value, made with `V()` if the key is new |
| `find(key)` | a pointer to the value, or `nullptr` |
| `at(key)` | the value, or throws `std::out_of_range` |
| `contains(key)` | whether `key` is stored |
| `erase(key)` | removes it and any branch nobody else uses; `true` if it was there |
| `size()`, `empty()`, `clear()` | |
| `has_prefix(prefix)` | whether any key starts with `prefix` |
| `count_prefix(prefix)` | how many keys start with `prefix`, without walking them (every branch keeps a count) |
| `keys_with_prefix(prefix, limit)` | those keys in order, at most `limit` |
| `entries_with_prefix(prefix, limit)` | the same with pointers to their values |
| `for_each_prefix(prefix, fn)` | calls `fn(key, value)` for each, in order; return `false` from `fn` to stop |
| `for_each(fn)`, `keys()` | every key |
| `complete(prefix)` | `prefix` plus as much as every key starting with it shares, like a shell's Tab |
| `longest_prefix_of(text)` | the longest key that `text` starts with: its length and value |
| `nearest(word, max_distance, limit)` | keys within `max_distance` edits (insert, delete, change a letter) of `word`, closest first, for "did you mean" |

`TrieSet` has the same without values: `insert(word)`, `contains`, `erase`, `has_prefix`, `count_prefix`, `with_prefix(prefix, limit)`, `words()`, `complete`, `longest_prefix_of` (the length), `nearest` and `for_each_prefix`.

## Ignoring case

The second template argument folds each letter before it's stored or looked up. `fizmo::collections::TrieIgnoreCase` folds A to Z to lowercase, so `"Vector"`, `"vector"` and `"VECTOR"` are one key (stored in lowercase):

```cpp
fizmo::TrieSet<fizmo::collections::TrieIgnoreCase> names{ "Vector", "VALUE" };
names.contains("vector");     // true
names.with_prefix("V");       // {"value", "vector"}
```

Any type with `char operator()(char) const` works as a fold.

## Cost

Looking up, inserting and erasing take time proportional to the key's length, not to how many keys there are. Listing keys with a prefix costs the length of the prefix plus the size of what's listed. Each node keeps its children in a small sorted array, so memory stays close to one node per distinct letter position.
