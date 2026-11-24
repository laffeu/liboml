# liboml
A fast, powerful, cross-platform library for parsing OML configuration files. It reads included files concurrently and falls back to buffered I/O when needed. Use append: to combine settings across files while rejecting duplicate keys, or override: to replace individual values in nested sections. Arrays and multiline strings keep complex configurations readable, while typed getters provide direct access to integers, floating-point numbers, and booleans

## Build & Install
```bash
cmake -S . -DBUILD_SHARED_LIBS=OFF
cmake --build . --target install
```
