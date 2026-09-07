default: build

setup:
    meson setup build

build:
    meson compile -C build

test: build
    meson test -C build --print-errorlogs

run *args: build
    ./build/barec {{args}}

fmt:
    ninja -C build clang-format

check: build
    ninja -C build clang-tidy
