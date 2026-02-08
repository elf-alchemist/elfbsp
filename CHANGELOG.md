Changes since ELFBSP 1.1
========================

New features:
* Added `--analysis` CLI parameter for BSP tree data analysis and visualization
* Added `--debug-*` CLI parameters for runtime debugging, previously a compile-time directive, now a runtime option
* Added support for the DeePBSPV4, XGLN and XGL2 node formats, now enforced via the `--type` CLI parameter

Bugfixes:
* Improved correctness for certain special effects, numerical effects of number 998 & 999 are now read from the line's tag instead of line's special
