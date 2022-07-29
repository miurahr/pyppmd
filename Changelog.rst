================
PyPPMd ChangeLog
================

All notable changes to this project will be documented in this file.

`Unreleased`_
=============

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
.. _Unreleased: https://github.com/miurahr/pyppmd/compare/v1.0.0...HEAD
.. _v1.0.0: https://github.com/miurahr/pyppmd/compare/v0.18.3...v1.0.0
.. _v0.18.3: https://github.com/miurahr/pyppmd/compare/v0.18.2...v0.18.3
.. _v0.18.2: https://github.com/miurahr/pyppmd/compare/v0.18.1...v0.18.2
.. _v0.18.1: https://github.com/miurahr/pyppmd/compare/v0.18.0...v0.18.1
.. _v0.18.0: https://github.com/miurahr/pyppmd/compare/v0.17.4...v0.18.0
