## solidc_hash: A Solidc Hash Function Library

This library provides various non-cryptographic hash function implementations for use in Solidc projects. 

Hash functions are one-way functions that convert arbitrary data into a fixed-size string of characters. 

**These are NOT suitable for cryptographic purposes**. 
> For cryptographic purposes, use a library like OpenSSL or Libsodium.

They are useful for tasks like:
* Creating unique identifiers for data
* Verifying data integrity
* Implementing hash tables

This library offers the following hash functions:

* **DJB2**: Daniel J. Bernstein's popular hash function.
* **FNV1a**: Fowler-Noll-Vo hash function variant.
* **ELF Hash**: A hash function commonly used in ELF object files.
* **DJB2a**: A variant of DJB2 with slightly different properties.
* **SDBM**: A simple hash function with good distribution properties.
* **CRC32**: A widely used error-detecting code.
* **MurmurHash**: A family of fast, high-quality hash functions. This implementation provides MurmurHash3 for 32-bit results.
* **XXH32**: An extremely fast hash function with good collision resistance.

## Usage

Each hash function takes two arguments:

* `key`: The data to be hashed (const void* pointer)
* For hash functions with variable input lengths:
    * `len`: The length of the input data (size_t)
    * `seed`: A seed value for the hash function (uint32_t)

The functions return a 32-bit unsigned integer (`uint32_t`) representing the hash value.

Here's an example of using the DJB2 hash function:

```c
#include "<solidc/solidc_hash.h>"

int main() {
  const char* str = "hello";
  uint32_t hash = solidc_djb2_hash(str);
  printf("DJB2 hash of '%s': %u\n", str, hash);
  return 0;
}
```

Using XXH32 with a seed value.
    
```c
#include "<solidc/solidc_hash.h>"

int main() {
  const char* str = "A very long string to hash";
  uint32_t seed = 0xABCD1234; // random seed value

  // XXH32 is very fast and has good collision resistance
  uint32_t hash = solidc_XXH32(str, strlen(str), seed);
  printf("XXH32 hash of '%s' with seed %u: %u\n", str, seed, hash);
  return 0;
}
```

## License

This library is licensed under the [MIT License](https://opensource.org/licenses/MIT).

## Contributing
We welcome contributions to this library.
