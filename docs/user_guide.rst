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

:mod:`pyppmd` Application programming interface
===============================================

Exception
---------

.. py:exception:: PpmdError

    This exception is raised when an error occurs.



Simple compression/decompression
--------------------------------

    This section contains:

        * function :py:func:`compress`
        * function :py:func:`decompress`


.. py:function:: compress(bytes_or_str: Union[bytes, bytearray, memoryview, str], max_order: int, mem_size: int)

    Compress *bytes_or_str*, return the compressed data.

    :param bytes_or_str: Data to be compressed. When it is type of str, encoded with "UTF-8" encoding before compress.
    :type bytes_or_str: bytes-like object or str
    :param max_order: maximum order of PPMd algorithm
    :type max_order: int
    :param mem_size: memory size used for building PPMd model
    :type mem_size: int
     :return: Compressed data
    :rtype: bytes

.. sourcecode:: python

    compressed_data = compress(data)

.. py:function:: decompress_str(data: Union[bytes, memoryview], max_order: int, mem_size: int, encoding: Optional[str])

    Decompress *data*, return the decompressed text.

    When encoding specified, return the decoded data as str type by specified encoding.
    Otherwise it returns data decoding by default "UTF-8".

    :param data: Data to be decompressed.
    :type data: bytes-like object
    :param max_order: maximum order of PPMd algorithm
    :type max_order: int
    :param mem_size: memory size used for building PPMd model
    :type mem_size: int
    :param encoding: Encoding name to use when decoding raw decompressed data
    :type encoding: str
    :return: Decompressed text
    :rtype: str
    :raises PpmdError: If decompression fails.

.. sourcecode:: python

    decompressed_text = decompress_str(data)


.. py:function:: decompress(data: Union[bytes, memoryview], max_order: int, mem_size: int)

    Decompress *data*, return the decompressed data.

    :param data: Data to be decompressed
    :type data: bytes-like object
    :param max_order: maximum order of PPMd algorithm
    :type max_order: int
    :param mem_size: memory size used for building PPMd model
    :type mem_size: int
    :return: Decompressed data
    :rtype: bytes
    :raises PpmdError: If decompression fails.

.. sourcecode:: python

    decompressed_data = decompress(data)


.. _stream_compression:

Streaming compression
---------------------

.. py:class:: PpmdCompressor

    A streaming compressor. It's thread-safe at method level.

    .. py:method:: __init__(self, max_order: int, mem_size: int)

        Initialize a PpmdCompressor object.

        :param max_order: maximum order of PPMd algorithm
        :type max_order: int
        :param mem_size: memory size used for building PPMd model
        :type mem_size: int

    .. py:method:: compress(self, data)

        Provide data to the compressor object.

        :param data: Data to be compressed.
        :type data: bytes-like object
        :return: A chunk of compressed data if possible, or ``b''`` otherwise.
        :rtype: bytes

    .. py:method:: flush(self)

        Flush any remaining data in internal buffer.

        The compressor object can not be used after this method is called.

        :return: Flushed data.
        :rtype: bytes

    .. sourcecode:: python

        c = PpmdCompressor()

        dat1 = c.compress(b'123456')
        dat2 = c.compress(b'abcdef')
        dat3 = c.flush()

Streaming decompression
-----------------------

.. py:class:: PpmdDecompressor

    A streaming decompressor. Thread-safe at method level.

    .. py:method:: __init__(self, max_order: int, mem_size: int)

        Initialize a PpmdDecompressor object.

        :param max_order: maximum order of PPMd algorithm
        :type max_order: int
        :param mem_size: memory size used for building PPMd model
        :type mem_size: int

    .. py:method:: decompress(self, data, max_length=-1)

        Decompress *data*, returning decompressed data as a ``bytes`` object.

        :param data: Data to be decompressed.
        :type data: bytes-like object
        :param int max_length: Maximum size of returned data. When it's negative, the output size is unlimited. When it's non-negative, returns at most *max_length* bytes of decompressed data. If this limit is reached and further output can (or may) be produced, the :py:attr:`~PpmdDecompressor.needs_input` attribute will be set to ``False``. In this case, the next call to this method may provide *data* as ``b''`` to obtain more of the output.

    .. py:attribute:: needs_input

        If the *max_length* output limit in :py:meth:`~PpmdDecompressor.decompress` method has been reached, and the decompressor has (or may has) unconsumed input data, it will be set to ``False``. In this case, pass ``b''`` to :py:meth:`~PpmdDecompressor.decompress` method may output further data.

        If ignore this attribute when there is unconsumed input data, there will be a little performance loss because of extra memory copy.

    .. py:attribute:: eof

        ``True`` means the end of the first frame has been reached. If decompress data after that, an ``EOFError`` exception will be raised.

    .. py:attribute:: unused_data

        A bytes object. When PpmdDecompressor object stops after end mark, unused input data after the end mark. Otherwise this will be ``b''``.

    .. sourcecode:: python

        d1 = PpmdDecompressor()

        decompressed_dat = d1.decompress(dat1)
        decompressed_dat += d1.decompress(dat2)
        decompressed_dat += d1.decompress(dat3)



:mod:`pyppmd` Low level Programming Interfaces
==============================================

There are FOUR classes to handle bare PPMd data.

Ppmd7Encoder and Ppmd7Decoder classes are designed to use as internal
class for py7zr, python 7-zip compression/decompression library.
Ppmd7Encoder and Ppmd7Decoder use a modified version of PPMd var.H
that use the range coder from 7z.

Ppmd8Encoder and Ppmd8Decoder classes are intend to use
general purpose text compression.

It uses `end mark` of source, when output text has ``\x01\x00``,
decompression is end, and ``\x01`` raw data is escaped as ``\x01\x01``.
This is a similar way with RAR archiver but is not compatible.

Ppmd8Encoder object
-------------------

.. class:: Ppmd8Encoder(max_order: int, mem_size: int)

    Encoder for PPMd Var.I. The ``max_order`` parameter is between 2 to 64.
    ``mem_size`` is a memory size in bytes which the encoder use.

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

.. class:: Ppmd8Decoder(max_order: int, mem_size: int)

    Decoder for PPMd Var.I. The ``max_order`` parameter is between 2 to 64.
    ``mem_size`` is a memory size in bytes which the encoder use.

    These parameters should as same as one when encode the data.

.. method:: Ppmd8Decoder.decode(data: Union[bytes, bytearray, memoryview], length: int)

   decode the given data and returns decoded data.
   When length is -1, maximum output data may be returned.

   If decoder got the end mark, decode() method automatically flush all data and close
   some resource. When reached to end mark, ``Ppmd8Decoder.eof`` member become True.

   When ``Ppmd8Decoder.needs_input`` is True, all input data is exhausted and
   need more input data to generate output. Otherwise, there are some data in internal
   buffer and reusable.

   The decoder may return data which size is smaller than specified length, that is
   because size of input data is not enough to decode.


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
