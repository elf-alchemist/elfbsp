Changes since ELFBSP 1.1
========================

New features:
* Added `--analysis` CLI parameter for BSP tree data analysis and visualization
* Added `--debug-*` CLI parameters for runtime debugging, previously a compile-time directive, now a runtime option
* Added support for the DeePBSPV4, XGLN and XGL2 node formats, now enforced via the `--type` CLI parameter
* Added support for special effects in Hexen-format maps via the line special 108

Bugfixes:
* Improved correctness for certain special effects, numerical effects of number 998 & 999 are now read from the line's tag instead of line's special
* Improved internal handling of special effects, now being determined when reading certain map formats, i.e special effects that only apply on Doom-, Hexen- or UDMF formats, will no longer "leak" onto the other formats

Meta:
* Added nightly builds on the GitHub Releases page, for easy access to development builds
* Ported various documenttaion files from other builders
* Moved to only building with LLVM Clang on all supported systems, now all three of the Windows, Mac OS and Linux builds use Clang.
