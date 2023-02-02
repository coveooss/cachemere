configure cfg="Debug" dir="build":
    cmake -B {{dir}} -S . -DUSE_VCPKG=ON -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE={{cfg}}

build dir="build":
    cmake --build {{dir}}

test cfg="Debug" dir="build":
    @just configure {{cfg}} {{dir}}
    @just build {{dir}}
    {{dir}}/tests/cachemere_tests
