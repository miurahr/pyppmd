.. _contributor_guide:

:tocdepth: 2

*****************
Contributor guide
*****************

Development environment
=======================

If you’re reading this, you’re probably interested in contributing to pyppmd.
Thank you very much! The purpose of this guide is to get you to the point
where you can make improvements to the py7zr and share them with the rest of the team.


Setup Python and C compiler
---------------------------

The ppmd is written in the Python and C languages bound with both CFFI, C Foreign
Function Interface, and CPython C/C++ API.
CFFI is used for PyPy3 and CPython API is used for CPython.

Python installation for various platforms with various ways.
You need to install Python environment which support `pip` command.
Venv/Virtualenv is recommended for development.

We have a test suite with python 3.8 and pypy3.
If you want to run all the test with these versions and variant on your local,
you should install these versions. You can run test with CI environment on
Github actions.


Get Early Feedback
------------------

If you are contributing, do not feel the need to sit on your contribution
until it is perfectly polished and complete. It helps everyone involved
for you to seek feedback as early as you possibly can.
Submitting an early, unfinished version of your contribution
for feedback in no way prejudices your chances of getting that contribution accepted,
and can save you from putting a lot of work into a contribution that is not suitable for the project.


Code Contribution
=================

Steps submitting code
---------------------

When contributing code, you’ll want to follow this checklist:

1. Fork the repository on GitHub.

2. Run the tox tests to confirm they all pass on your system. If they don’t, you’ll need
   to investigate why they fail. If you’re unable to diagnose this yourself,
   raise it as a bug report.

3. Write tests that demonstrate your bug or feature. Ensure that they fail.

4. Make your change.

5. Run the entire test suite again using tox, confirming that all tests pass
   including the ones you just added.

6. Send a GitHub Pull Request to the main repository’s master branch.
   GitHub Pull Requests are the expected method of code collaboration on this project.

Code review
-----------

Contribution will not be merged until they have been code reviewed. There are limited
reviewer in the team, reviews from other contributors are also welcome.
You should implemented a review feedback unless you strongly object to it.


Code style
----------

The pyppmd uses the PEP8/Black code style. In addition to the standard PEP8,
we have an extended guidelines.

* line length should not exceed 125 characters.
* Black format prettier is enforced.
* It also use MyPy static type check enforcement.


Test cases
----------

There is three types of tests and we measures coverages;

* Unit tests for encode and decode, single data, and multiple data.
* Integration test with CSV file which size is larger than buffer size.
* Hypothesis fuzzing test.

All tests should be passed before merging.
