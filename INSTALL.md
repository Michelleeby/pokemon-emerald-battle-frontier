# Developer build instructions

Use these instructions to build Pokémon Emerald Battle Frontier from source
with the legacy, byte-matching `agbcc` toolchain. This workflow has been
verified on Ubuntu 24.04 under WSL2.

## Requirements

Install the host compiler, build tools, ARM binutils, Git, and libpng headers:

```sh
sudo apt-get update
sudo apt-get install -y build-essential binutils-arm-none-eabi git libpng-dev
```

Clone this repository and `pret/agbcc` into the same parent directory. The
commands below use this layout:

```text
pokedecomps/
├── agbcc/
└── pokeemerald/
```

## Set up the compiler

From the parent directory, build `agbcc`, then install its generated
compiler and libraries into this project. The ROM build uses this installed
copy:

```sh
cd agbcc
./build.sh
./install.sh ../pokeemerald
```

Repeat this step whenever you replace or rebuild the `agbcc` checkout so the
project uses the rebuilt compiler and libraries. Do not commit the generated
files under `pokeemerald/tools/agbcc`.

## Build

```sh
cd ../pokeemerald
make
```

The build produces `pokeemerald.gba`. The current default `agbcc`
source-build checksum is documented in the README. Release checksums belong
to release artifacts; a local source build can differ as the source tree and
build inputs change.

Calculate the SHA-1 with:

```sh
sha1sum pokeemerald.gba
```

You may see `pkg-config: No such file or directory` while compiling host
tools. If the tools still link successfully against libpng, this message does
not prevent the build from completing.

For a clean rebuild, run `make clean` to remove generated ROMs, objects, host
tools, converted assets, and other build output. Then run `make -j2`.

## Tests

Run the gameplay suites linked against production code:

```sh
make check-all
```

Validate the test manifests, then run the suite selector unit tests:

```sh
python3 tools/testing/validate_manifest.py
python3 -m unittest discover -s tools/testing -p 'test_*.py' -v
```

The end-to-end tests also require the pinned sibling mGBA checkout. See
[tests/README.md](tests/README.md) for the gameplay and end-to-end (E2E)
workflow.

## Vanilla reference

The upstream vanilla `pret/pokeemerald` base has this SHA-1:

```text
f3ae088181bf583e55daf962a92bb46f4f1d07b7
```

`make compare` checks a clean vanilla checkout against that hash. Since this
project changes the game, its ROM is expected to have a different hash. To
verify a release ROM, use the `checksums.txt` shipped with that release.

Neither the source repository nor its releases provide original or patched ROM
files.
