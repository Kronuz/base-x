# Parser fuzzing

Configure with Homebrew LLVM and instrument the library as well as the harness:

```sh
cmake -S . -B build-fuzz -DBUILD_FUZZING=ON -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-O1 -g -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all"
quiet light base-x-fuzz-build 10 "Build parser fuzzer" -- cmake --build build-fuzz --target base_x_fuzzer -j4
mkdir -p .scratch/corpus
cp fuzz/corpus/* .scratch/corpus/
quiet light base-x-fuzz 30 "Parser fuzzing" -- build-fuzz/fuzz/base_x_fuzzer .scratch/corpus -seed=20261113 -max_len=2048 -max_total_time=1700 -print_final_stats=1
```

Run repeated bounded chunks until the process user and system CPU times total at least 3,600 seconds. Keep generated corpus and crash artifacts in `.scratch/`; the checked-in corpus contains stable seeds.
