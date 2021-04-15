try:
    from .c.c_ppmd import *  # noqa
except ImportError:
    try:
        from .cffi.cffi_ppmd import *  # noqa
    except ImportError:
        msg = "pyppmd module: Neither C implementation nor CFFI " "implementation can be imported."
        raise ImportError(msg)

__doc__ = """\
Python bindings to PPMd compression library, the API is similar to
Python's bz2/lzma/zlib module.

Documentation: https://pyppmd.readthedocs.io
Github: https://github.com/miurahr/pyppmd
PyPI: https://pypi.org/prject/pyppmd"""
