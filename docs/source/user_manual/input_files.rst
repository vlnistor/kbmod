Input Files
===========

KBMOD expects Vera C. Rubin Science Pipelines calexp-style FITS files. These are multi-extension fits files that contain:

* photometrically and astometrically calibrated single-CCD image, usually refered to as the "science image",
* variance image, representing per-pixel noise levels, and a
* pixel bitmask

stored in 1st, 2nd and 3rd header extension/plane respectively. The zeroth header extension is expected to contain the image metadata. A collection of science images that overlap the same area on the sky at different times are expected to be grouped into directories, usually refered to as "pointing groups". The path to this directory is a required input to KBMOD, see :ref:`Search Parameters`.

The images are expected to be warped, i.e. geometrically transformed to a set of images with a consistent and uniform relationship between sky coordinates and image pixels on a shared pixel grid. 

Visit ID
--------

In order to associate input files with auxiliary data, such as time stamps or PSFs, each visit uses a unique numeric ID. This ID string can be provided in the ``IDNUM`` field of the FITS file’s header 0. If no ``IDNUM`` field is provided, then KBMOD will attempt to derive the visit ID from the file name as described in the next section.

Naming Scheme
-------------

Each file **must** include ``.fits`` somewhere in the file name. Additionally the file names can be used to encode the visit ID. If no ``IDNUM`` field is provided, KBMOD will look for a contiguous sequence of five or more numeric digits in the file name. If found, the first such sequence is used as the visit ID. For example a file name “my12345.fits” will map to the visit ID “12345”.

Time file
---------

There are two cases where you would want to use an external time file:

* when the FITS files do not contain timestamp information
      If no file is included, KBMOD will attempt to extract the timestamp from the FITS file header (in the MJD field).
* when you want to prefilter the files based on the parameter ``mjd_lims`` (see :ref:`Search Parameters`) before loading the file.
      This reduces loading time when accessing a large directory.

The time file provides a mapping of visit ID to timestamp. The time file is an ASCII text file containing two space-separated columns of data: the visit IDs and MJD time of the observation. The first line is a header denoted by ``#``::

    # visit_id mean_julian_date
    439116 57162.42540605324
    439120 57162.42863899306
    439124 57162.43279313658
    439128 57162.436995358796
    439707 57163.41836675926
    439711 57163.421717488425



PSF File
--------

The PSF file is an ASCII text file containing two space-separated columns of data: the visit IDs and variance of the PSF for the corresponding observation. The first line is a header denoted by ``#``::

    # visit_id psf_val
    439116 1.1
    439120 1.05
    439124 1.4

A PSF file is needed whenever you do not want to use the same default value for every image.


Data Loading
------------

Data is loaded using :py:meth:`~kbmod.analysis_utils.Interface.load_images`. The method creates an :py:class:`~kbmod.search.ImageStack` object, which is a collection of :py:class:`~kbmod.search.LayeredImage` objects. Each :py:class:`~kbmod.search.LayeredImage` contains the PSF, mask and the science image while :py:class:`~kbmod.search.ImageStack` tracks the properties that apply to all images in the collection, such as global masks etc. The :py:class:`~kbmod.search.ImageStack` will include only those images that with observation timestamps within the given MJD bounds.

The :py:meth:`~kbmod.analysis_utils.Interface.load_images` method also returns helper information:
 * ``img_info`` - An object containing auxiliary data from the fits files such as their WCS and the location of the observatory.
