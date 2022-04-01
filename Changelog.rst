================
PyPPMd ChangeLog
================

All notable changes to this project will be documented in this file.

`Unreleased`_
=============

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
.. _Unreleased: https://github.com/miurahr/pyppmd/compare/v0.18.2...HEAD
.. _v0.18.2: https://github.com/miurahr/pyppmd/compare/v0.18.1...v0.18.2
.. _v0.18.1: https://github.com/miurahr/pyppmd/compare/v0.18.0...v0.18.1
.. _v0.18.0: https://github.com/miurahr/pyppmd/compare/v0.17.4...v0.18.0
