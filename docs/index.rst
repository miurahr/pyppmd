PyPPMd Python module
====================

PPM, Prediction by partial matching, is a wellknown compression technique
based on context modeling and prediction. PPM models use a set of previous
symbols in the uncompressed symbol stream to predict the next symbol in the
stream.

PPMd is an implementation of PPMII by Dmitry Shkarin.

The ``pyppmd`` package uses core C files from ``p7zip``.
The library has a bare function and no metadata/header handling functions.
This means you should know compression parameters and input/output data
sizes.

This library implements PPMd Variant H, and PPMd Variant I Version 2.


.. toctree::
   :maxdepth: 2
   :caption: Contents:

   getting_started
   api_guide
   ppmd8
   ppmd7
   contribution
   authors

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
