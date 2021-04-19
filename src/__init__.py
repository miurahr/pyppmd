try:
    from importlib.metadata import PackageNotFoundError, version
except ImportError:
    from importlib_metadata import PackageNotFoundError, version  # type: ignore  # noqa

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
PyPI: httpsrecursive-include src *.pyi://pypi.org/prject/pyppmd"""

__copyright__ = "Copyright (C) 2020,2021 Hiroshi Miura"

try:
    __version__ = version(__name__)
except PackageNotFoundError:  # pragma: no-cover
    # package is not installed
    __version__ = "unknown"
