PyPPMd
======


.. image:: https://readthedocs.org/projects/pyppmd/badge/?version=latest
  :target: https://pyppmd.readthedocs.io/en/latest/?badge=latest

.. image:: https://badge.fury.io/py/pyppmd.svg
  :target: https://badge.fury.io/py/pyppmd

.. image:: https://img.shields.io/conda/vn/conda-forge/pyppmd
  :target: https://anaconda.org/conda-forge/pyppmd

.. image:: https://github.com/miurahr/pyppmd/workflows/Run%20Tox%20tests/badge.svg
  :target: https://github.com/miurahr/pyppmd/actions

.. image:: https://coveralls.io/repos/github/miurahr/pyppmd/badge.svg?branch=main
  :target: https://coveralls.io/github/miurahr/pyppmd?branch=main


Introduction
------------

``pyppmd`` module provides classes and functions for compressing and decompressing text data,
using PPM(Prediction by partial matching) compression algorithm which has several variations of implementations.
PPMd is the implementation by Dmitry Shkarin.
PyPPMD use Igor Pavlov's range coder introduced in 7-zip.

The API is similar to Python's bz2/lzma/zlib module.

Some parts of th codes are derived from ``7-zip``, ``pyzstd`` and ``ppmd-cffi``.


Development status
------------------

A development status is considered as ``Beta``.

Extra input byte
----------------

``PPMd`` algorithm and implementation is designed to use ``Extra`` input byte.
The encoder will omit a last null (b"\0") byte when last byte is b"\0".
You may need to provide an extra null byte when you don't get expected size of
extracted data.

You can do like as:

.. code-block::

    dec = pyppmd.Ppmd7Decoder(max_order=6, mem_size=16 << 10)
    result = dec.decode(compressed, length)
    if len(result) < length:
        if dec.needs_input:
            # ppmd need an extra null byte
            result += dec.decode(b"\0", length - len(result))
        else:
            result += dec.decode(b"", length - len(result))


WARNING
-------

* There is a known issues for Decoder that sometimes failed with access violation error
  on 32bit python on Windows.
* It is recommended to use PyPPMd on 64bit python.

Copyright and License
---------------------

Copyright (C) 2020-2022 Hiroshi Miura

Copyright (C) 2020-2021 Ma Lin

Copyright (C) 2010-2012 Lockless Inc.

Copyright (C) 1999-2017 Igor Pavlov

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
