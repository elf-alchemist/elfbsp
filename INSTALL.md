Compiling ELFBSP
===============

The ELFBSP code is fairly portable C++, and does not depend
on any third-party libraries.  It requires at least C++11.
Both GNU g++ and clang++ are known to work.

Building should be fairly straight-forward on any Unix-like
system, such as Linux, the BSDs, and even MacOS X.  Fairly
simple Makefiles are used for building, although they require
GNU make, so on BSD systems you will need the 'gmake' package.

On Windows, building can be done via CMake and Visual Studio.
Alternatively, you can use a cross compiler on Linux.  See
below for more information.


Linux / BSD
-----------

On Debian linux, you will need the following packages:

- g++
- binutils
- cmake
- make

It is a good idea to open the Makefile in a text editor and
check various settings.  For example, you may want to change
the `PREFIX` variable, or set `CXX` to a specific C++ compiler.

To build the program, type:

    cmake -B build && make all -C build

To install ELFBSP, for which you will need root priveliges, type:

    cmake -B build && sudo make install -C build


Windows
-------

Using CMake or CMake-GUI, you can generate a Microsoft Visual Studio
solution for Windows.  Simply point CMake to the root CMakeLists.txt
inside of /ELFbsp, and generate the solution for your intended target
machine and version of Visual Studio.  Upon a successful build,
MSVC will generate `elfbsp.exe` in your build directory of choice.

For detailed information, see [INSTALL-Win.md](INSTALL-Win.md)


Cross Compiling
---------------

A "cross compiler" is a compiler which builds binaries for
one platform (e.g. Windows) on another platform (e.g. Linux).
This is how the current Windows executable was built.

On Debian linux, the following packages are needed to build
the Windows binary:

- mingw-w64
- make

To build the program, type:

    make -f Makefile.xming

The executable can be made smaller using the following:

    make -f Makefile.xming stripped
