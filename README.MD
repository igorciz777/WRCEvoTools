# WRC Evo Tools
A set of tools for files found in WRC games developed by Evolution Studios on the PS2 platform.

## Features
Currently, the tool can:
- Extract the contents of `DVD.DAT` and assign filenames to the extracted files based on the listed hash in `DVD.BIN` and the currently known filenames listed in `src/filenames.hpp`
- Hash filenames and look up names from hashes
- Build new `DVD.DAT` and `DVD.BIN` files from a input directory
- Import a single file back into `DVD.DAT` (if not larger than the original)
- Decompress WRC 2-5 ELFs to a standard ELF file.

Planned features include:
- Solving the encryption/compression algorithm found in EVO container files.
- Full reimporting and rebuilding support for EVO data files.

## Usage

```shell
.\evo_tools [cmd] [file|text]
```

### Modes

- **-hash [text]**: Hash provided text using the custom hash found in the games.
- **-name [hash]**: Get filename from hash value.
- **-udvd [DVD.BIN] [DVD.DAT] [out dir]**: Extract DVD.DAT to a specified output directory.
- **-rdvd [DVD.BIN] [DVD.DAT] [input dir]**: Build new DVD files from a directory.
- **-idvd [DVD.BIN] [DVD.DAT] [input file]**: Reimport a file (cannot be bigger than original).
- **-delf [packed ELF] [out ELF]**: Decompress WRC 2-5 ELF.

### Examples

Extracting `DVD.DAT` file contents to the `out` directory:
```shell
.\evo_tools -udvd DVD.BIN DVD.DAT .\out
```

Hash a filename:
```shell
.\evo_tools -hash somefile.dat
```

Get filename from hash:
```shell
.\evo_tools -name 12345678
```

Build DVD files from a directory:
```shell
.\evo_tools -rdvd DVD.BIN DVD.DAT .\input_dir
```

Import a single file back into DVD.DAT:
```shell
.\evo_tools -idvd DVD.BIN DVD.DAT somefile.dat
```

Decompress the ELF:
```shell
.\evo_tools -delf ".\WRC Rally Evolved\SCES_532.47" .\SCES_532.47_decrypted.ELF --scratchpad-memsz 0x3F91
```

## Game compatibility table

|                     **Name**                      | **Serial** | **Extract DVD.DAT/BIN** | **Build DVD.DAT/BIN** | **Import to DVD.DAT/BIN** | **Decompress ELF** |
|:-------------------------------------------------:|:----------:|:-------------------:|:-----------------:|:----------------:|:-----------------:|
| WRC World Rally Championship                      | SCES-50139 |  :question: | :question:       | :question:       | :question:       |
| WRC II Extreme                                    | SCES-50934 |  :heavy_check_mark: | :question:       | :question:       | :heavy_check_mark: |
| WRC II Extreme (Japan)                            | SLPM 65284 |  :heavy_check_mark: | :question:       | :question:       | ships decompressed |
| WRC 3                                             | SCES-51684 |  :heavy_check_mark: | :question:       | :question:       | :question:       |
| WRC 3 (Japan)                                     | SLPM 65583 |  :heavy_check_mark: | :question:       | :question:       | :heavy_check_mark: |
| WRC 3 (OPS2M Demo)                                | SCEE-52092 |  :heavy_check_mark: | :question:       | :question:       | :question:       |
| WRC 4 1.01                                        | SCES-52389 |  :heavy_check_mark: | :question:       | :question:       | :question:       |
| WRC 4 2.00                                        | SCES-52389 |  :heavy_check_mark: | :question:       | :question:       | :question:       |
| WRC 4 2.00 (Japan)                                | SLPM 65975 |  :heavy_check_mark: | :question:       | :question:       | :heavy_check_mark: |
| WRC: Rally Evolved                                | SCES-53247 |  :heavy_check_mark: | :question:       | :question:       | :heavy_check_mark: |
| WRC avec Sébastien Loeb Édition 2005              | SCES-53680 |  :heavy_check_mark: | :question:       | :question:       | ships decompressed |

## Building

```shell
g++ -o evo_tools src/main.cpp
```