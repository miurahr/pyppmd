================
PyPPMd ChangeLog
================

All notable changes to this project will be documented in this file.

`Unreleased`_
=============

v1.3.1_
=======

Fixed
-----
* Fix publish CI/CD configuration
    * Bump musllinux image musllinux_1_2
    * Bump manylinux image manylinux_2_28

v1.3.0_
=======

Fixed
-----
* Fix several issues in ThreadDecoder.c (#126)
    * Fix the double call of Ppmd7_Free from both Ppmd7T_Free and Ppmd7Decoder_dealloc
    * Fix the double call of Ppmd8_Free from both Ppmd8T_Free and Ppmd8Decoder_dealloc
* Fix the issue in PyPY (#126)
    * Fix initialization order in ffi_build.py
    * Fix eof handling in cffi_ppmd.py

Added
-----
* Add support for Python 3.14

Changed
-------
* Add compile and link flag for building C++ with `-pthread` (#126)
* Minimum required python to be 3.10

v1.2.0_
=======

Added
-----
* Add Windows on Arm64, Linux on aarch64 support

Changed
-------
* Released from GitHub Actions as trusted platform

v1.1.1_
=======

Added
-----
* Add Python 3.13 support

Changed
-------
* Minimum required python to be 3.9


`v1.1.0`_
=========

Added
-----
* Add Python 3.12 support

Changed
-------
* Minimum required python to be 3.8
* Export PPMD8_RESTORE_METHOD_* constants
* Drop setup.cfg
* Drop github actions workflows
* README: Add SPDX identifier
* CI run on python 3.10, 3.11 and 3.12


`v1.0.0`_
=========

Changed
-------
* Fix publish script to make sdist and upload it.
* Move CI on Azure pipelines
* Migrate forge site to CodeBerg.org
* Drop release-note and stale actions

`v0.18.3`_
==========

Added
-----
* Release wheel for python 3.11 beta

Fixed
-----
* CI: update setuptools before test run (#115)
* CI: fix error on tox test on aarch64.

Changed
-------
* Bump pypa/cibuildwheel@2.7.0 (#116)
* Bump actions/setup-python@v4 (#114)
* Bump actions/download-artifact, actions/upload-artifact@v3 (#105,#106)
* CI: Test with python 3.11 beta(#112)
* Update license notifications
* Move C sources under ``src/lib`` folder

`v0.18.2`_
==========

Fixed
-----
* Publish wheel package for python 3.10 on macos.
* pyproject.toml: add "version" as dynamic (#100)

Changed
-------
* Update security policy to support version to be 0.18.x
* Move old changelog to Chanlog.old.rst


`v0.18.1`_
==========

Fixed
-----
* Installation error with recent pip version (#94, #95)
  * Add metadata in pyproject.toml
* PPMd8: check double flush(#96)

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


.. History links
.. _Unreleased: https://github.com/miurahr/pyppmd/compare/v1.3.1...HEAD
.. _v1.3.1: https://github.com/miurahr/pyppmd/compare/v1.3.0...v1.3.1
.. _v1.3.0: https://github.com/miurahr/pyppmd/compare/v1.2.0...v1.3.0
.. _v1.2.0: https://github.com/miurahr/pyppmd/compare/v1.1.1...v1.2.0
.. _v1.1.1: https://github.com/miurahr/pyppmd/compare/v1.0.0...v1.1.1
.. _v1.1.0: https://github.com/miurahr/pyppmd/compare/v1.0.0...v1.1.0
.. _v1.0.0: https://github.com/miurahr/pyppmd/compare/v0.18.3...v1.0.0
.. _v0.18.3: https://github.com/miurahr/pyppmd/compare/v0.18.2...v0.18.3
.. _v0.18.2: https://github.com/miurahr/pyppmd/compare/v0.18.1...v0.18.2
.. _v0.18.1: https://github.com/miurahr/pyppmd/compare/v0.18.0...v0.18.1
.. _v0.18.0: https://github.com/miurahr/pyppmd/compare/v0.17.4...v0.18.0
