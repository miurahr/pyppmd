.. _ppmd7:

Ppmd7 Objects
=============

Ppmd7Encoder and Ppmd7Decoder classes are designed to use as internal
class for py7zr, python 7-zip compression/decompression library.
Ppmd7Encoder and Ppmd7Decoder use a modified version of PPMd var.H
that use the range coder from 7z.


.. py:class:: Ppmd7Encoder

   Encoder for PPMd Variant H.

.. py:method:: __init__(max_order: int, mem_size: int)

   The ``max_order`` parameter is between 2 to 64.
   ``mem_size`` is a memory size in bytes which the encoder can use.

.. py:method:: Ppmd7Encoder.encode(data: Union[bytes, bytearray, memoryview])

   Compress data, returning a bytes object containing compressed data for
   at least part of the data in data. This data should be concatenated to
   the output produced by any preceding calls to the encode() method.
   Some input may be kept in internal buffers for later processing.

.. py:method:: Ppmd7Encoder.flush(endmark: boolean)

   All pending input is processed, and bytes object containing the remaining
   compressed output is returned. After calling flush(), the encode() method
   cannot be called again; the only realistic action is to delete the object.
   When ``endmark`` is true, flush write endmark(-1) to end of archive, otherwise
   do not write (default).

.. py:class:: Ppmd7Decoder

   Decoder for PPMd Variant H.

.. py:method:: __init__(max_order: int, mem_size: int)

   The ``max_order`` parameter is between 2 to 64.
   ``mem_size`` is a memory size in bytes which the encoder can use.

.. py:method:: Ppmd7Decoder.decode(data: Union[bytes, bytearray, memoryview], length: int)

   returns decoded data that sizes is length.

   decoder may return data which size is smaller than specified length, that is because
   size of input data is not enough to decode.

.. py:method:: Ppmd7Decoder.flush(length: int)

   All pending input is processed, and a bytes object containing the remaining uncompressed
   output of specified length is returned. After calling flush(), the decode() method
   cannot be called again; the only realistic action is to delete the object.
