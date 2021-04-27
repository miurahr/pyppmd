.. _user_guide:

**********
User Guide
**********

PPM, Prediction by partial matching, is a wellknown compression technique
based on context modeling and prediction. PPM models use a set of previous
symbols in the uncompressed symbol stream to predict the next symbol in the
stream.

PPMd is an implementation of PPMII by Dmitry Shkarin.

The ``pyppmd`` package uses core C files from ``p7zip``.
The library has a bare function and no metadata/header handling functions.
This means you should know compression parameters and input/output data
sizes.


Getting started
===============

Install
-------

The pyppmd is written by Python and C language bound with both CFFI and CPython C/C++ API,
and can be downloaded from PyPI(aka. Python Package Index) using standard 'pip' command
as like follows;

.. code-block:: bash

    $ pip install pyppmd


:mod:`pyppmd` Programming Interfaces
====================================

There are FOUR classes to handle bare PPMd data.

Ppmd7Encoder and Ppmd7Decoder classes are designed to use as internal
class for py7zr, python 7-zip compression/decompression library.
Ppmd7Encoder and Ppmd7Decoder use a modified version of PPMd var.H
that use the range coder from 7z.

Ppmd8Encoder and Ppmd8Decoder classes are intend to use
general purpose text compression.

It use `end mark` of source, when output text has ``\x01\x00``,
decompression is end, and ``\x01`` raw data is escaped as ``\x01\x01``.
This is a similar way with RAR archiver but is not compatible.

Ppmd8Decoder also accept ``length`` parameter how many bytes should
generated.

Ppmd8Encoder object
-------------------

.. class:: Ppmd8Encoder(max_order: int, mem_size: int, end_mark: boolean)

    Encoder for PPMd Var.I. The ``max_order`` parameter is between 2 to 64.
    ``mem_size`` is a memory size in bytes which the encoder use.
    ``end_mark`` is a flag whether encoder escape ``\x01`` and use ``\x00``
    for end mark. Default is ``False``.

.. method:: Ppmd8Encoder.encode(data: Union[bytes, bytearray, memoryview])

    compress data, returning a bytes object containing copressed data.
    This data should be concatenated to the output produced by any
    preceding calls to the encode().
    Some input may be kept in internal buffer for later processing.

.. method:: Ppmd8Encoder.flush()

    All pending input is processed, and bytes object containing the remaining
    compressed output is returned. After calling flush(), the encode() method
    cannot be called again; the only realistic action is to delete the object.
    flush() method releases some resource the object used.

Ppmd8Decoder object
-------------------

.. class:: Ppmd8Decoder(max_order: int, mem_size: int, end_mark: boolean)

    Decoder for PPMd Var.I. The ``max_order`` parameter is between 2 to 64.
    ``mem_size`` is a memory size in bytes which the encoder use.
    ``end_mark`` is a flag whether decoder escape ``\x01`` and use ``\x00``
    for end mark. Default is ``False``

    These parameters should as same as one when encode the data.

.. method:: Ppmd8Decoder.decode(data: Union[bytes, bytearray, memoryview], length: int)

   decode the given data and returns decoded data.
   When end mark mode specified, and length is -1, then maximum output data is returned.

   If decoder got the end mark, decode() method automatically flush all data and close
   some resource. When reached to end mark, ``Ppmd8Decoder.eof`` member.

   When there is not an end mark mode, length should be positive integer.

   The decoder may return data which size is smaller than specified length, that is
   because size of input data is not enough to decode.


.. method:: Ppmd8Decoder.flush(length: int)

   When end mark mode, length is ignored.
   When data has already flushed, it just returns b""

   All pending input is processed, and a bytes object containing the remaining uncompressed
   output of specified length is returned. After calling flush(), the decode() method
   cannot be called again; the only realistic action is to delete the object.


Ppmd7Encoder object
-------------------

.. class:: Ppmd7Encoder(max_order: int, mem_size: int)

   Encoder for PPMd Var.H. The ``max_order`` parameter is between 2 to 64.
   ``mem_size`` is a memory size in bytes which the encoder can use.

.. method:: Ppmd7Encoder.encode(data: Union[bytes, bytearray, memoryview])

   Compress data, returning a bytes object containing compressed data for
   at least part of the data in data. This data should be concatenated to
   the output produced by any preceding calls to the encode() method.
   Some input may be kept in internal buffers for later processing.

.. method:: Ppmd7Encoder.flush()

   All pending input is processed, and bytes object containing the remaining
   compressed output is returned. After calling flush(), the encode() method
   cannot be called again; the only realistic action is to delete the object.


Ppmd7Decoder object
-------------------

.. class:: Ppmd7Decoder(max_order: int, mem_size: int)

   Decoder for PPMd Var.H. The ``max_order`` parameter is between 2 to 64.
   ``mem_size`` is a memory size in bytes which the encoder can use.

.. method:: Ppmd7Decoder.decode(data: Union[bytes, bytearray, memoryview], length: int)

   returns decoded data that sizes is length.

   decoder may return data which size is smaller than specified length, that is because
   size of input data is not enough to decode.

.. method:: Ppmd7Decoder.flush(length: int)

   All pending input is processed, and a bytes object containing the remaining uncompressed
   output of specified length is returned. After calling flush(), the decode() method
   cannot be called again; the only realistic action is to delete the object.

.. Note:: mem_size parameter should be as bytes not MB.
