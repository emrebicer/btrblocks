# BtrBlocks - Efficient Columnar Compression for Data Lakes

[![Paper](https://img.shields.io/badge/paper-SIGMOD%202023-green)](https://bit.ly/btrblocks)
[![Build](https://github.com/maxi-k/btrblocks/actions/workflows/test.yml/badge.svg?event=push)](https://github.com/maxi-k/btrblocks/actions/workflows/test.yml)

- [Paper](https://bit.ly/btrblocks) (two-column version)
- [Video](https://dl.acm.org/doi/10.1145/3589263) (SIGMOD 2023 presentation)

## Usage

After [building](#building) the library, follow one of the [examples](./tools/examples) to get started.

## Components

- `btrblocks/`: the compression library, schemes, utilities, ...
- `btrfiles/`: helper library for binary files and yaml schema information
- `tools/`: various conversion, measurement and benchmarking tools
- `test/`: rudimentary tests for the library

![Dependency Graph](./doc/dependencies.svg)

## Building

We currently require an x86 platform.
This library was built and tested on linux only.

``` sh
mkdir build 
cd build
cmake ..
```

Then, depending on your usecase, build only the library or any of the tools:
- build everything: `make`
- install static library and headers on your system: `sudo make install`
- build the compression library only: `make btrblocks`
- build the tests `make tester`
- build the in-memory decompression speed benchmark: `make decompression_speed`
- ...



### Building the example
After creating and building necessarry libraries, go to build directory and run

```bash
g++ ./CMakeFiles/example_compression.dir/tools/examples/compression.cpp.o \
                                            -L./ -lbtrblocks \
                                            -L./vendor/croaring/src/croaring_src-build -lroaring \
                                            -L./vendor/lemire/fastpfor/src/fastpfor_src-build/ -lFastPFOR \
                                            -L./vendor/cwida/fsst/src/fsst_src-build -lfsst \
                                            -Wl,-rpath=./vendor/croaring/src/croaring_src-build \
                                            -o compression_example_executable
```

### Running the csvtobtr and btrtocsv binaries
Example usage;
```bash
./csvtobtr -create_btr -csv <path to csv file> -yaml <path to yaml file> -binary binary/ -create_binary

./btrtocsv -btr btr/ -csv output.csv
```

The yaml file should represent the csv columns and data types, e.g.;
```yaml
columns:
  - name: Index
    type: integer
    
  - name: Customer Id
    type: string

  - name: Rating
    type: double
```

For a list of all valid targets, run `make help`.

Library was built and tested on Linux (x86, ARM) and MacOS (ARM).

## Contributors

Adnan Alhomssi
David Sauerwein
Maximilian Kuschewski

## License

MIT - See [License File](LICENSE)
