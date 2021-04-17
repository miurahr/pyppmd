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

There are two classes to handle bare PPMd data.


Ppmd7Encoder object
-------------------

.. class:: Ppmd7Encoder(max_order: int, mem_size: int)

   Encoder for PPMd Ver.H. The ``max_order`` parameter is between 2 to 64.
   ``mem_size`` is a memory size in bytes which the encoder can use.

.. method:: Ppmd7Encoder.encode(data: Union[bytes, bytearray, memoryview])

   Compress data, returning a bytes object containing compressed data for
   at least part of the data in data. This data should be concatenated to
   the output produced by any preceding calls to the encode() method.
   Some input may be kept in internal buffers for later processing.

.. method:: Ppmd7Encoder.flush()

   All pending input is processed, and a bytes object containing the remaining
   compressed output is returned. After calling flush(), the encode() method
   cannot be called again; the only realistic action is to delete the object.


Ppmd7Decoder object
-------------------

.. class:: Ppmd7Decoder(max_order: int, mem_size: int)

   Decoder for PPMd Ver.H. The ``max_order`` parameter is between 2 to 64.
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
