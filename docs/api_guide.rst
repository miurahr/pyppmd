.. _api_guide:


Application programming interface
=================================

Exception
---------

.. py:exception:: PpmdError

    This exception is raised when an error occurs.



Simple compression/decompression
--------------------------------

    This section contains:

        * function :py:func:`compress`
        * function :py:func:`decompress`


.. py:function:: compress(bytes_or_str: Union[bytes, bytearray, memoryview, str], max_order: int, mem_size: int, variant: str)

    Compress *bytes_or_str*, return the compressed data.

    :param bytes_or_str: Data to be compressed. When it is type of str, encoded with "UTF-8" encoding before compress.
    :type bytes_or_str: bytes-like object or str
    :param max_order: maximum order of PPMd algorithm
    :type max_order: int
    :param mem_size: memory size used for building PPMd model
    :type mem_size: int
    :param variant: PPMd variant name, only accept "H" or "I"
    :type variant: str
    :return: Compressed data
    :rtype: bytes

.. sourcecode:: python

    compressed_data = compress(data)

.. py:function:: decompress_str(data: Union[bytes, memoryview], max_order: int, mem_size: int, encoding: str, variant: str)

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
    :param variant: PPMd variant name, only accept "H" or "I"
    :type variant: str
    :return: Decompressed text
    :rtype: str
    :raises PpmdError: If decompression fails.

.. sourcecode:: python

    decompressed_text = decompress_str(data)


.. py:function:: decompress(data: Union[bytes, memoryview], max_order: int, mem_size: int, variant: str)

    Decompress *data*, return the decompressed data.

    :param data: Data to be decompressed
    :type data: bytes-like object
    :param max_order: maximum order of PPMd algorithm
    :type max_order: int
    :param mem_size: memory size used for building PPMd model
    :type mem_size: int
    :param variant: PPMd variant name, only accept "H" or "I"
    :type variant: str
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

    .. py:method:: __init__(self, max_order: int, mem_size: int, variant: str, restore_method: int)

        Initialize a PpmdCompressor object. restore_method param is affected only when variant is "I".

        :param max_order: maximum order of PPMd algorithm
        :type max_order: int
        :param mem_size: memory size used for building PPMd model
        :type mem_size: int
        :param variant: PPMd variant name, only accept "H" or "I"
        :type variant: str
        :param restore_method: PPMD8_RESTORE_METHOD_RESTART(0) or PPMD8_RESTORE_METHOD_CUTOFF(1)
        :type restore_method: int

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

    A streaming decompressor. Thread-safe at method level. A restore_method param is affected only when variant is "I".

    .. py:method:: __init__(self, max_order: int, mem_size: int, variant: str, restore_method: int)

        Initialize a PpmdDecompressor object.

        :param max_order: maximum order of PPMd algorithm
        :type max_order: int
        :param mem_size: memory size used for building PPMd model
        :type mem_size: int
        :param variant: PPMd variant name, only accept "H" or "I"
        :type variant: str
        :param restore_method: PPMD8_RESTORE_METHOD_RESTART(0) or PPMD8_RESTORE_METHOD_CUTOFF(1)
        :type restore_method: int

    .. py:method:: decompress(self, data, max_length=-1)

        Decompress *data*, returning decompressed data as a ``bytes`` object.

        :param data: Data to be decompressed.
        :type data: bytes-like object
        :param int max_length: Maximum size of returned data. When it's negative, the output size is unlimited. When it's non-negative, returns at most *max_length* bytes of decompressed data. If this limit is reached and further output can (or may) be produced, the :py:attr:`~PpmdDecompressor.needs_input` attribute will be set to ``False``. In this case, the next call to this method may provide *data* as ``b''`` to obtain more of the output.

    .. py:attribute:: needs_input

        If the *max_length* output limit in :py:meth:`~PpmdDecompressor.decompress` method has been reached,
        and the decompressor has (or may has) unconsumed input data, it will be set to ``False``.
        In this case, pass ``b''`` to :py:meth:`~PpmdDecompressor.decompress` method may output further data.

        If ignore this attribute when there is unconsumed input data, there will be a little performance loss because of extra memory copy.
        This flag can be True even all input data are consumed, when decompressor can be able to accept more data in some case.

    .. py:attribute:: eof

        ``True`` means the end of the first frame has been reached.
        If decompress data after that, an ``EOFError`` exception will be raised.
        This flag can be False even all input data are consumed, when decompressor can be able to accept more data in some case.

    .. py:attribute:: unused_data

        A bytes object. When PpmdDecompressor object stops after end mark, unused input data after the end mark. Otherwise this will be ``b''``.

    .. sourcecode:: python

        d1 = PpmdDecompressor()

        decompressed_dat = d1.decompress(dat1)
        decompressed_dat += d1.decompress(dat2)
        decompressed_dat += d1.decompress(dat3)

