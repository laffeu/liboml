# liboml
mm,m
## Library features

- Nested sections, arrays, and multiline strings
- Typed getters for integers, floating-point numbers, and booleans
- `append:` adds settings from other files and rejects duplicates
- `override:` replaces existing values, including values in nested sections
- Concurrent config reads with a buffered file I/O fallback

## Build & Install
```bash
cmake -S .
cmake --build . --target install
```
