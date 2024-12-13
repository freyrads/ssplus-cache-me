# SSPlus Cache Me

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
