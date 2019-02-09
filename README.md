Separate Chaining Hash Table
============================

A C++17 `std::unordered_map`/`std::unordered_set` compatible replacement that can be tuned for memory efficiency or/and speed.

The hash table uses separate chaining for collision resolution.
The hash table maintains several buckets for storing elements.
In case that the hash table is initialized as a hash map, a bucket consists of a key and a value array, called `key_bucket` and `value_bucket`.
For the hash set case, there is no `value_bucket`.

## Motivation

In this hash table, a bucket storing keys and values uses to separate arrays, i.e., an array for the keys and an array for the values.
This layout makes it possbile to store keys and values more efficiently in case that they are plain integers with a bit width smaller than the word size (< 64 bits).
First, a `std::pair<key,value>` is not stored byte-packed, using memory that is a multiple of the word size.
Second, finding a key in a large `key_bucket` is faster than in a bucket storing `std::pair<key,value>` elements, since more keys can fit into the cache line.


## Usage

The library is header-only and can be included with `#include <separate_chaining_map.hpp>`.

The hash table is called `separate_chaining_table`. 
There are two typedefs `separate_chaining_map` and `separate_chaining_set` for creating a `unordered_map` or `unordered_set`, respectively.
The template parameters are:

- `key_bucket_t<key_t>` the type of bucket to store keys of type `key_t`. These are defined in `bucket.hpp`, and are
  - `class_key_bucket` for the most general case
  - `plain_key_bucket` for the case that the keys can be copied with `std::memcpy` (complicated classes with copy constructors must be maintained in a `class_key_bucket`)
  - `avx2_key_bucket` for the case that the keys are integers and that the CPU supports the AVX2 instruction set.
  - `varwidth_key_bucket` for the case that the keys are integers and that there is an arbitrary maximum bit width of the integers to store. This is beneficient in combination with a compact hash function (see below). However, operations on this bucket take more time.

- `value_bucket_t` the type of bucket to store values. Possible classes are `class_key_bucket` and `plain_key_bucket`, which can also store values instead of keys. 
Like in the description above, use `class_key_bucket` for non-`std::memcpy`-able value types.

- `hash_mapping_t` is a mapping that takes a key and returns, based on the number of the buckets of the hash table, a pair consisting of a hash-value and a remainder.
  - The hash value is an integer in `[0, bucket_count()-1]`. 
  - The remainder is defined such that the keys can be restored from the pair. In the most simple case, the remainder is the key. 
    This fact is used by the class `hash_mapping_adapter<key_t, hash_function>` that turns a standard hash function `hash_funtion` hashing `key_t` elements into a `hash_mapping_t`.
	Since the hash table can restore a key by having its hash value `v` and its remainder `r`, it stores `r` in the `v`-th bucket instead of the key.
A non-trivial mapping is `xorshift_hash`. Here, the bit width of the remainder is the bit width of the remainder minus `log2(bucket_count())`. 
In conjunction with `varwidth_key_bucket`, this fact can be used to represent the keys in less bits than their bit width.
This technique is also called quotienting.
Another way is to allocate sufficiently large memory to store keys of a specific bit width in a `plain_key_bucket` or `avx2_key_bucket` storing remainders with a smaller bit width. For instance, 24-bit keys can be stored in `plain_key_bucket<uint8_t>` if there are `2^{24} / 2^{8} = 2^16 = 65536` buckets.

- `resize_strategy_t` defines what to do when inserting an element in a full bucket.
  - `incremental_resize` lets a bucket grow incrementally, which means that a bucket is always full, and that each insertion results in a `malloc` call.
    On the bright side, each bucket takes exactly as much memory as needed. 
    Moreover, there is no need to store the capacities of the buckets.  
    and that there is no space wasted.
  - `arbitrary_resize` increases the size of a bucket in relation to its current size. The current implementation doubles the size `s` of a bucket until `s` is larger than a fixed threshold. 
  After surpassing the threshold, the size is increased merely by 50%. This behavior can be adjusted in `arbitrary_resize::resize`.

## Key Splitting

Given the bit widths of the keys has an interesting distribution, it is also possible to combine several hash tables with an adapter, such that each hash table is 
responsible for keys whose bit width are within a given range.
This adapter allows each hash table to adjust the needed space for the quotient.
The class `keysplit_adapter<map_type, max_bits, m_length>` is a hash map adapter for storing keys up to bit width `max_bits`.
Therefore, it uses an array of `map_type` hash maps. This array has length `m_length`. 
The range `[0...max_bits]` is equi-distantly distributed to the hash maps of this array.
However, since `xorshift_hash` does not work with integers having a bit width of 64, 
there is also a class `keysplit_adapter64<map_type, large_map_type, m_length>` that wraps around `keysplit_adapter<map_type, 63, m_length>`.
This class uses the map `large_map_type` for storing keys with bit width 64.

## Chaining Bucket

For small data sets (< 100 elements), it is faster and more memory efficient to use a single bucket without hashing by relying on large caches during the linear scanning process.
The class `chaining_bucket` wraps a single bucket in a map/set interface. 

## Usage
- Elements can be searched with `find`
- The map can be used with the handy []-operator for retrieving and writing values. 
- `find_or_insert` can be used to insert a key with a default value, or retrieve this key's value if it has already been inserted.

## Implementation Details

- A bucket stores initially `INITIAL_BUCKETS` many buckets. This constant is defined for each `resize_strategy_t` differently. For `arbitrary_resize`, this size can be arbitrarily chosen.
- A bucket can grow up to `MAX_BUCKET_BYTESIZE` elements. This value is linked with the type `bucketsize_type` representing integers up to `MAX_BUCKET_BYTESIZE`.
- `erase` does not free memory. For that, use `fit_to_shrink`.
- The typedefs for hash map and hash sets wrap the value bucket type `value_bucket_t` around a manager for the array of value buckets, which is either realized by `value_array_manager` using the straight-forward way (for emulating a hash map), or `value_dummy_manager` for storing no value at all (for emulating a hash set).



## Dependencies

- Command line tools
  - cmake
  - make
  - a C++17 compiler like gcc or clang 
  - gtest (optional) for tests
  - celero (optional) for benchmarking

## Outlook
It is possible to extend this approach for Cuckoo Hashing with multiple hash functions.

## Related Work
- [bonsai tables](https://github.com/tudocomp/compact_sparse_hash): A set of hash tables using compact hashing and linear probing for collision resolution. 
  The linear probing makes it difficult to maintain a remainder with hash value `v` at position `v` in the hash table. For that additional maintaince is required that needs extra space and slows down the computation. On the other hand, it does not need buckets, and therefore can maintain elements more efficiently if the space requirement is known in advance.
