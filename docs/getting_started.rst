.. _getting_started:

Getting started
===============

Install
-------

The pyppmd is written by Python and C language bound with both CFFI and CPython C/C++ API,
and can be downloaded from PyPI(aka. Python Package Index) using standard 'pip' command
as like follows;

.. code-block:: console

    pip install pyppmd


When installing on CPython, it downloads a wheel with CPython C/C++ extension.
When installing on PyPY, it downloads a wheel with CFFI extension.
There are binaries for CPython 3.6, 3.7, 3.8, 3.9 on Windows(32bit, 64bit), macOS and Linux(amd64, aarch64),
and PyPy7.3(python 3.7) on macOS, Windows(64bit), and Linux(32bit, aarch64).