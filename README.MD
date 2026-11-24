# WRC Evo Tools
A set of tools for files found in WRC games developed by Evolution Studios on the PS2 platform.

## Features
Currently, the tool can:
- Extract the contents of `DVD.DAT` and assign filenames to the extracted files based on the listed hash in `DVD.BIN` and the currently known filenames listed in `src/filenames.hpp`
- Unpack EVO data files (`EVO9` / `EVOA` / `EVOB`) into their decompressed blocks
- Rebuild a generic uncompressed (`RAW0`) EVO data file from decompressed data
- Hash filenames and look up names from hashes
- Build new `DVD.DAT` and `DVD.BIN` files from a input directory
- Import a single file back into `DVD.DAT` (if not larger than the original)
- Decompress WRC 2-5 ELFs to a standard ELF file.
- Write a patched ISO copy that boots the decompressed ELF.

Planned features include:
- Full reimporting and compression support for EVO data files (current rebuild is uncompressed).

## Usage

```shell
.\evo_tools [cmd] [file|text]
```

### Modes

- **-hash [text]**: Hash provided text using the custom hash found in the games.
- **-name [hash]**: Get filename from hash value.
- **-udvd [DVD.BIN] [DVD.DAT] [out dir] [--evo]**: Extract DVD.DAT to a specified output directory. `--evo` also unpacks every EVO data file found during extraction.
- **-rdvd [DVD.BIN] [DVD.DAT] [input dir]**: Build new DVD files from a directory.
- **-idvd [DVD.BIN] [DVD.DAT] [input file]**: Reimport a file (cannot be bigger than original).
- **-udat [datafile] [out dir]**: Unpack an EVO data file. Writes each block as `<name>.decompressed` into the output directory. `RAW0` blocks are copied as-is.
- **-rdat [decompressed file] [out dat]**: Rebuild an EVO data file from decompressed data. Writes an `EVOB` container with a single uncompressed `RAW0` block.
- **-delf [packed ELF] [out ELF]**: Decompress WRC 2-5 ELF.
- **-diso [in ISO] [out ISO]**: Write a copy of the ISO that boots the decompressed ELF. The input ISO is not modified.

### Examples

Extracting `DVD.DAT` file contents to the `out` directory:
```shell
.\evo_tools -udvd DVD.BIN DVD.DAT .\out
```

Extract `DVD.DAT`, but also unpack every EVO data file found while extracting (file is replaced by its decompressed version):
```shell
.\evo_tools -udvd DVD.BIN DVD.DAT .\out --evo
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

Unpack an EVO data file:
```shell
.\evo_tools -udat somefile.dat .\out
```

Rebuild an EVO data file from decompressed data:
```shell
.\evo_tools -rdat .\out\somefile.dat.decompressed .\somefile.dat
```

Decompress the ELF:
```shell
.\evo_tools -delf ".\WRC Rally Evolved\SCES_532.47" .\SCES_532.47_decrypted.ELF --scratchpad-memsz 0x3F91
```

Write a patched ISO that boots the decompressed ELF:
```shell
.\evo_tools -diso ".\dumps\SCES_532.47.WRC Rally Evolved.iso" .\WRC5_patched.iso --scratchpad-memsz 0x3F91
```

## Game compatibility table

|                     **Name**                      | **Serial** | **Extract DVD.DAT/BIN** | **Build DVD.DAT/BIN** | **Import to DVD.DAT/BIN** | **Decompress ELF** |
|:-------------------------------------------------:|:----------:|:-------------------:|:-----------------:|:----------------:|:-----------------:|
| WRC World Rally Championship                      | SCES-50139 |  :question: | :question:       | :question:       | ships decompressed |
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