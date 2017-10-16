# EFI Boot Guard #

A bootloader based on UEFI.

Provides the following functionality:
* Arm a hardware watchdog prior to loading an OS
* Provides a simple update mechanism with fail-save algorithm

## Development ##

Mailing list:
[efibootguard-dev@googlegroups.com](efibootguard-dev@googlegroups.com)

Archive:
[https://www.mail-archive.com/efibootguard-dev@googlegroups.com/](https://www.mail-archive.com/efibootguard-dev@googlegroups.com)

For sending patches, please refer to the mailing list and `CONTRIBUTING.md` in
the source tree.

Continuous integration:
* [Travis CI](https://travis-ci.org/siemens/efibootguard):
  * Master branch: ![](https://img.shields.io/travis/siemens/efibootguard/master.svg)
  * Next branch: ![](https://img.shields.io/travis/siemens/efibootguard/next.svg)
* Coverity: ![](https://img.shields.io/coverity/scan/13885.svg)

## Watchdog support ##

The following watchdog drivers are implemented:
* Intel Quark
* Intel TCO
* Intel i6300esb

Currently, it is not possible to disable the watchdog initialization. If no
working watchdog is found, the boot process fails.

## Configuration ##

`efibootguard` reads its configuration from an environment storage. Currently,
the following environment backends are implemented:
* Dual FAT Partition storage

See `Installation And Usage` for further information.

## Further Documentation ##

* [Update Mechanism](docs/UPDATE.md)
* [Environment Tools](docs/TOOLS.md)
* [API Library](docs/API.md)
* [Compilation Instructions](docs/COMPILE.md)
* [Installation And Usage](docs/USAGE.md)
* [Future Work](docs/TODO.md)
