# Development

## Build

### Meson

We're using [meson](https://mesonbuild.com/) ❤️ which is a front-end to ninja ❤️.[^1]

The build configuration is defined in `meson.build` files, which explicitely include subdir configurations:

    meson.build              # subdir('src')
    src/meson.build          # subdir('net/kad')
    src/net/kad/meson.build
    …

We mainly build the main executable `ptp` and tests executables.

A main lib `libptp.a` is used to share code between the main executable and
tests.

We use 2 sets of Kad constants: `net/kad/.full` for the normal implementation
and `net/kad/.tiny` used in some tests to simplify reasoning.
The Kad implementation is thus built into 2 libs:

- `libptpkad.a` integrated into the main lib;
- `libptpkadalt.a` integrated into a lib `libptpalt.so`[^2], itself used in few
  tests.

### static vs shared lib

**FIXME** We should probably aim for a single shared main lib:

- avoid embedding main code in all tests executables;
- potentially make kad code available to other programs; plus no need to build
a shared version, plus an additional static one for the main executable.

### Usage

Quick start:

    mkdir build && cd build
    meson
    ninja

    ninja -C build  # from project root

Reset:

    meson setup --wipe
    ninja clean         # delete built

Multiple builds:

    mkdir build-clang && cd build-clang
    CC=clang meson

Configure build:

    meson configure -Db_coverage=true -Db_sanitize=address,undefined

## Tests


Tests and linting are applied as a git pre-commit hook. Git hooks installed
upon meson setup (`meson.build`).

### Unit tests

Each source compilation unit (`.c` file) should have a corresponding unit test
in `tests/`. For example `src/net/kad/routes.c` would have a
`tests/net/kad/routes.c`[^3].

Unit tests are run in parallel.

    cd build
    meson test  # or `ninja test`

Specific tests

    meson test "unit/*"
    meson test --list
    meson test unit/kad/routes.c

See [meson doc](https://mesonbuild.com/Unit-tests.html) for more options.

### Integration tests

**2025-08 ipv6 is required**: `sysctl net.ipv6.conf.lo.disable_ipv6=0`

In `tests/integration/`. One python runner per test suite or use case.

    meson test "integration/*"

A test usually starts a ptp server and checks its interaction with it.

#### Valgrind

**Please run manually** from time to time.

    meson test --wrapper='valgrind --trace-children=yes'
    valgrind --leak-check=full --show-leak-kinds=all --trace-children=yes \
        tests/integration/send-to-server routes_state build/src/ptp -c tests/kad/data

## Lint

We use *cppcheck* (`tools/cppcheck-run`).

    ninja cppcheck

> 2025-08 cppcheck fails to complete and thus disabled in pre-commit.
> **Meanwhile please run it manually**

## Coverage

We use *gcov* (`tools/coverage`).

    meson configure -Db_coverage=true  # REQUIRED
    meson test
    ninja cover
    firefox cover/index.html

> 2025-08 meson provides coverage targets (like `coverage-html`) but they don't
> seem to be easily configurable.

We don't have a minimum coverage target yet.

## Installation

> This area is [absolutely unexplored](https://mesonbuild.com/Installing.html).

`data/nodes.dat` contains the bootstrap nodes.

## Code

### Style

- General rule: **look around and mimic the existing style**.
- Docstrings / code documentation roughly follows
  [doxygen](https://www.doxygen.nl/manual/docblocks.html). In particular:
  - Function arguments are mentioned with `@` (ex: `@param1`).
- Argument order: destination then source
- Naming: from most generic to most specific; name then verb. So things, and
  especially functions, are easily sorted.

### Practices

- Favor stack allocation to heap allocation.

## Documentation

* `doc/development.md`
* `doc/internals.md`
* `doc/man/ptp.1.in`


[^1]: We're proud to be pretty early adopters of meson, with `0.37.0.dev1`,
    when it was not packaged on distros and the best was to embed it into the repo.

[^2]: This is just the result of experimenting with meson capabilities.

[^3]: We do cut corners and may skip unit testing in favor of integration
    tests, for example when a lot of setup is needed.
