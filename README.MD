# WRC Evo Tools
A set of tools for files found in WRC games developed by Evolution Studios on the PS2 platform.
## Features
Currently, the tool only extracts the contents of `DVD.DAT` and assigns filenames to the extracted files based on the listed hash
in `DVD.BIN` and the currently known filenames listed in `src/filenames.hpp`

Planned features include:
- Solving the encryption/compression algorithm found in EVO container files.
- Reimporting files back into `DVD.DAT`.


## Usage
```shell
.\evo_tools [cmd] [file|text]
```
### Modes
- **-h [text]**: Hash provided text using the custom hash found in the games.
- **-dvd [DVD.BIN] [DVD.DAT] [out dir]**: Extract DVD.DAT to a specified output directory.

### Examples
Extracting `DVD.DAT` file contents to the `out` directory:
```shell
.\evo_tools -dvd DVD.BIN DVD.DAT .\out
```

## Game compatibility table

|                     **Name**                    | **Serial** | **DVD.DAT Extract** |
|:-----------------------------------------------:|:----------:|:-------------------:|
| WRC II Extreme                             | SCES-50934 |  :heavy_check_mark: |
| WRC II Extreme (Japan)                     | SLPM 65284 |  :heavy_check_mark: |
| WRC 3                                | SCES-51684 |  :heavy_check_mark: |
| WRC 3 (Japan)                        | SLPM 65583 |  :heavy_check_mark: |
| WRC 3 (OPS2M Demo)                                | SCEE-52092 |  :heavy_check_mark: |
| WRC 4 1.01                              | SCES-52389 |  :heavy_check_mark: |
| WRC 4 2.00                      | SCES-52389 |  :heavy_check_mark: |
| WRC 4 2.00 (Japan)                      | SLPM 65975 |  :heavy_check_mark: |
| WRC: Rally Evolved                                | SCES-53247 |  :heavy_check_mark: |
| WRC avec Sébastien Loeb Édition 2005                  | SCES-53680 |  :heavy_check_mark: |

## Building

```shell
g++ -o evo_tools src/main.cpp
```