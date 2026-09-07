default: build

setup:
    meson setup build

build:
    meson compile -C build

test: build
    meson test -C build --print-errorlogs

run *args: build
    ./build/barec {{args}}

install: build
    meson install -C build

snapshots: build
    UPDATE_SNAPSHOTS=1 sh tests/snapshot.sh ./build/barec

fmt:
    ninja -C build clang-format

check: build
    ninja -C build clang-tidy
