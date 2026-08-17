# Developer build instructions

These instructions build Pokémon Emerald: Battle Frontier from source with the
legacy, byte-matching `agbcc` toolchain. The workflow has been verified on
Ubuntu 24.04 under WSL2.

## Requirements

Install the host compiler, build tools, ARM binutils, Git, and libpng headers:

```sh
sudo apt-get update
sudo apt-get install -y build-essential binutils-arm-none-eabi git libpng-dev
```

Clone this repository and `pret/agbcc` beside one another:

```text
pokedecomps/
├── agbcc/
└── pokeemerald/
```

## Set up the compiler

From the parent directory, build `agbcc` and install its generated compiler and
libraries into this project:

```sh
cd agbcc
./build.sh
./install.sh ../pokeemerald
```

Repeat this step after replacing or rebuilding the `agbcc` checkout. The
generated files under `pokeemerald/tools/agbcc` must not be committed.

## Build

```sh
cd ../pokeemerald
make -j2
```

The output is `pokeemerald.gba`. A successful production build has this SHA-1:

```text
94dd919efb13876a9bfa53a5787f3d294281f3a8
```

Verify it with:

```sh
sha1sum pokeemerald.gba
```

The occasional `pkg-config: No such file or directory` message while compiling
host tools is non-fatal when the tools still link successfully against libpng.

Use `make clean` to remove generated ROMs, objects, host tools, converted
assets, and other build output. Run `make -j2` afterward for a clean rebuild.

## Tests

Run the production-linked gameplay suites:

```sh
make check-all
```

Validate the suite selectors and their unit tests:

```sh
python3 tools/testing/validate_manifest.py
python3 -m unittest discover -s tools/testing -p 'test_*.py' -v
```

The end-to-end tests require the pinned sibling mGBA checkout. See
[tests/README.md](tests/README.md) for the complete gameplay and E2E workflow.

## Vanilla reference

The upstream vanilla `pret/pokeemerald` base has this SHA-1:

```text
f3ae088181bf583e55daf962a92bb46f4f1d07b7
```

`make compare` applies to a clean vanilla checkout. This modified project's ROM
is expected to differ from the vanilla hash.

Neither the source repository nor its releases provide original or patched ROM
files.
