# KBMOD (Kernel-Based Moving Object Detection)

[![Build Status](https://travis-ci.org/jbkalmbach/kbmod.svg?branch=master)](https://travis-ci.org/jbkalmbach/kbmod) [![License](https://img.shields.io/badge/License-BSD%202--Clause-orange.svg)](https://opensource.org/licenses/BSD-2-Clause)

A Maximum Likelihood detection algorithm for moving astronomical objects.

KBMOD is a set of Python tools to search astronomical images for moving
objects based upon method of maximum likelihood detection.

## Requirements

The packages required to run the code are:

* Numpy
* Scipy
* Scikit-learn
* Matplotlib
* Astropy

## Setup

Clone the repo and from bash use

```
source setup.bash
```

To setup the gpu program, enter the directory `code/gpu/debug/` and use
```
source appendPath.bash
```
then build the executable with
```
./eraseRebuild.sh
```
the search parameters can be speficied in `parameters.config`.

CudaTracker will launch the search
```
 ./CudaTracker
```


## Example

See the example [ipython notebook](https://github.com/jbkalmbach/kbmod/blob/master/notebooks/kbmod_demo.ipynb).

## License

The software is open source and available under the BSD license.