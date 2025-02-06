# SSPlus Cache Me

A clone of [auliawiguna/go-cache-me](https://github.com/auliawiguna/go-cache-me) in C++ with a little bit of differences.
Made for science.

## Compiling with Clang

```sh
# Init build dir
mkdir -p build
cd build

# Source provided clang env
source ../clang-env.sh

# Generate build files
cmake ..

# Compile
make all -j$(nproc)
```

## API Endpoints

Below is an overview of the available endpoints:

### 1. **POST** `/cache`

Creates a new cache entry.

**Payload:**
- `key`: (string) The unique identifier for the cache entry.
- `ttl`: (number) Time-to-live for the cache entry, a duration in millisecond eg. 600000 for 10 minutes.
- `value`: (string) The data to store in the cache.

### 2. **POST** `/cache/get-or-set`

Retrieves an existing cache entry or creates a new one if it does not exist.

**Payload:**
- `key`: (string) The unique identifier for the cache entry.
- `ttl`: (number) Time-to-live for the cache entry, e.g. 600000.
- `value`: (string) The data to store if the cache entry does not already exist.

If a cache entry with the specified key already exists, the existing value will be returned. Otherwise, a new entry is created.

### 3. **GET** `/cache/:key`

Fetches the cached data associated with the specified `key`. Returns the data if found, otherwise responds with an appropriate error.

### 4. **DELETE** `/cache/:key`

Deletes the cache entry associated with the specified `key`.
