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


Programming Interfaces
======================

There are two classes to handle bare PPMd data.

* Ppmd7Encoder(max_order, mem_size)
* Ppmd7Decoder(max_order, mem_size)

.. Note:: mem_size parameter should be as bytes not MB.
