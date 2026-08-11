# Native tests and fuzzers

The standalone test project exercises security-sensitive components without
requiring copyrighted game data or initializing the SDL runtime.

Configure and run the deterministic tests:

```sh
cmake -S Tests -B DerivedData/tests -G Ninja
cmake --build DerivedData/tests
ctest --test-dir DerivedData/tests --output-on-failure
```

On Linux with Clang, enable ASan/UBSan and the libFuzzer target:

```sh
CC=clang CXX=clang++ cmake -S Tests -B DerivedData/tests-sanitize -G Ninja \
  -DONS_ENABLE_SANITIZERS=ON -DONS_BUILD_FUZZERS=ON
cmake --build DerivedData/tests-sanitize
ctest --test-dir DerivedData/tests-sanitize --output-on-failure
DerivedData/tests-sanitize/fuzz_archive_parser -runs=10000
```

Fuzz findings should be minimized and retained as regression cases before a
fix is merged. The suite covers archive indexes, command-line validation,
serialized save data, the bounded legacy regular-expression matcher, and
multidimensional variable-copy correctness.
