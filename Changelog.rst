================
PyPPMd ChangeLog
================

All notable changes to this project will be documented in this file.

`Unreleased`_
=============

`v0.18.0`_
==========
Fixed
-----
* test: Fix fuzzer error with silent null byte (#89)
* test: 32bit test memory parameter too large(#90)
* PPMd7: avoid access violation on dealloc when failed in allocation (#91)
* PPMd7: decoder.eof and decoder.needs_input return proper value(#92)

Security
-----
* PPMd7,PPMd8: fix struct definition by include process.h in windows
  This may cause crash on 32bit version of python on windows(#86)

Changed
-------
* PPMd7: decompressor use threading(#85)

Added
-----
* doc: Explanation of ``Extra silent null byte`` in README


`v0.17.4`_
==========
Fixed
-----
* ppmd7: allow multiple decode without additional input data (#84)
* ppmd8: test: Fix fuzzer test program (#82)

Changed
-------
* CI: bump actions/checkout@v3 (#81)
* CI: bump actions/setup-python@v3 (#80)
* CI: bump pypa/cibuildwheel@2.3.1 (#78)
* Update 32bit detection logic

`v0.17.3`_
==========
Fixed
-----
* Build on MingW/MSYS2(#68,#69)

Added
-----
* Test on Python 3.10.0, PyPY-3.6 and PyPy-3.7 (#71)

Changed
-------
* CI: use pypa/ciwheelbuild(#70)
* CI: add dependabot(#70)
* Bump versions
  - CI: pypa/ciwheelbuild@2.2.2
  - CI: run-on-arch@2.1.1
  - CI: actions/stale@4
* CI: exclude pypy on windows
* CI: exclude cp310-macos because python 3.10 for macos is superceded
* CI: publish musllinux wheel
* CI: improve cibuildwheel performance

`v0.17.1`_
==========

Added
-----
* Wheels for python 3.10

`v0.17.0`_
==========

Added
-----
* unified API for variation H and I
* ppmd7, ppmd8: flag to control endmark(-1) addtions.
  defaults:  ppmd7 without endmark, ppmd8: with endmark.

Changed
-------
* Unified API to use Variant H, and Varant I version 2 from simple API.
  User can provide ``variant`` argument to the constractor. (#59)
* Allocate PPMD7Decompressor buffer variables from heap(#52)
* Replace pthread wrapper library to the verison of one made by Lockless. Inc. (#67)
* Refactoring internal variable namees, move thread shared variable into ThreadControl structure.

Fixed
-----
* More robust PPMd8Decompressor by taking thread control variables and buffers from heap,
  and remove global variables.(#54)
* PPMD8Decoder: Deadlock on Windows(#67 and more)

Deprecated
----------

Removed
-------
* End-mark (0x01 0x00) mode(#62)

Security
--------

`v0.16.1`_
==========

Added
-----
* CI: add macOS as test matrix(#51)

Fixed
-----
* Fix osX bulid error(#49,#50)

`v0.16.0`_
==========

Added
-----
* PPMd8: support endmark option(#39)
* PPMd8: support restore_method option(#24, @cielavenir)
* Add pthread wrapper for macOS and Windows(#33)

Changed
-------
* PPMd8: decompressor use threading(#24,#33)

Fixed
-----
* PPMd8: Decompressor become wrong status when memory_size is smaller than file size(#24,#25,#28,#33,#45,#46)
* PPMd8: Decompressor allocate buffers by PyMem_Malloc() (#42)
* CMake: support CFFI extension generation(#30)
* CMake: support debug flag for extension development(#27)
* CMake: support pytest_runner on windows
* CI: run tox test on pull_request

`v0.15.2`_
==========

Added
-----
* Add development note using cmake

Fixed
-----
* Make CMake build script working

Security
--------
* Hardening for multiplexing overflow(scan#1)

`v0.15.1`_
==========

Added
-----
* Badge for conda-forge package(#19)

Changed
-------
* Test document with tox

Fixed
-----
* Fix setup.py: pyppmd.egg-info/SOURCES.txt not including full path
* Fix source package not include .git* files(#20)
* Fix compiler warning by cast.

`v0.15.0`_
==========

* Now development status is Beta.

Added
-----

* Introduce PpmdCompressor and PpmdDecompressor class for stream compression.
* Introduce decompress_str() one-shot utility to return str object.

Changed
-------

* decompress() always return bytes object.

Deprecated
----------

* PPMd8: drop length mode for decompression and always use end mark mode.
* PPMd8: drop flush() method for decompression.

`v0.14.0`_
==========

Added
-----
* Introduce compress() and decompress() one-shot utility
  - compress() accept bytes-like object or string. When string, encode it to UTF-8 first.
  - decompress() has an argument encoding, if specified, it returns string.
* C: CFFI: Introduce End-Mark mode for PPMd8

Changed
-------
* C: Limit initial output buffer size as same as specified length.
* C: Allow python thread when decode/encode loop running.


`v0.13.0`_
==========

Added
-----
* Benchmark test to show performance

Changed
-------
* Change folder structures in source.
* Release resources on flush()

Fixed
-----
* Fix input buffer overrun(#8)

`v0.12.1`_
==========

Fixed
-----
* Fix dist of typing stubs


`v0.12.0`_
==========

Added
-----
* add PPMd varietion I (PPMd8)
  - Ppmd8Encoder, Ppmd8Decoder class
* MyPy typing stubs

Changed
-------
* switch to LGPLv2.1+ License
* Introduce flush() method for decode class.

Fixed
-----
* Fix build error on Windows.


`v0.11.1`_
==========

Fixed
-----
* Fix Packaging configuration

`v0.11.0`_
==========

Fixed
-----
* Better error handling for memory management.

Changed
-------
* Skip hypothesis tests on windows
* Limit hypothesis tests parameter under available memory.


v0.10.0
=======

* First Alpha

.. History links
.. _Unreleased: https://github.com/miurahr/pyppmd/compare/v0.18.0...HEAD
.. _v0.18.0: https://github.com/miurahr/pyppmd/compare/v0.17.4...v0.18.0
.. _v0.17.4: https://github.com/miurahr/pyppmd/compare/v0.17.3...v0.17.4
.. _v0.17.3: https://github.com/miurahr/pyppmd/compare/v0.17.1...v0.17.3
.. _v0.17.1: https://github.com/miurahr/pyppmd/compare/v0.17.0...v0.17.1
.. _v0.17.0: https://github.com/miurahr/pyppmd/compare/v0.16.1...v0.17.0
.. _v0.16.1: https://github.com/miurahr/pyppmd/compare/v0.16.0...v0.16.1
.. _v0.16.0: https://github.com/miurahr/pyppmd/compare/v0.15.2...v0.16.0
.. _v0.15.2: https://github.com/miurahr/pyppmd/compare/v0.15.1...v0.15.2
.. _v0.15.1: https://github.com/miurahr/pyppmd/compare/v0.15.0...v0.15.1
.. _v0.15.0: https://github.com/miurahr/pyppmd/compare/v0.14.0...v0.15.0
.. _v0.14.0: https://github.com/miurahr/pyppmd/compare/v0.13.0...v0.14.0
.. _v0.13.0: https://github.com/miurahr/pyppmd/compare/v0.12.1...v0.13.0
.. _v0.12.1: https://github.com/miurahr/pyppmd/compare/v0.12.0...v0.12.1
.. _v0.12.0: https://github.com/miurahr/pyppmd/compare/v0.11.1...v0.12.0
.. _v0.11.1: https://github.com/miurahr/pyppmd/compare/v0.11.0...v0.11.1
.. _v0.11.0: https://github.com/miurahr/pyppmd/compare/v0.10.0...v0.11.0
