"""A class for creating fake data sets.

The FakeDataSet class allows the user to create fake data sets
for testing, including generating images with random noise and
adding artificial objects. The fake data can be saved to files
or used directly.
"""

import os
import random
import numpy as np
from pathlib import Path

from astropy.io import fits

from kbmod.configuration import SearchConfiguration
from kbmod.data_interface import save_deccam_layered_image
from kbmod.file_utils import *
from kbmod.search import *
from kbmod.search import Logging
from kbmod.wcs_utils import append_wcs_to_hdu_header
from kbmod.work_unit import WorkUnit

def make_fake_layered_image(
    width,
    height,
    noise_stdev,
    pixel_variance,
    obstime,
    psf,
    seed=None,
):
    """Create a fake LayeredImage with a noisy background.

    Parameters
    ----------
        width : `int`
            Width of the images (in pixels).
        height : `int
            Height of the images (in pixels).
        noise_stdev: `float`
            Standard deviation of the image.
        pixel_variance: `float`
            Variance of the pixels, assumed uniform.
        obstime : `float`
            Observation time.
        psf : `PSF`
            The PSF for the image.
        seed : `int`, optional
            The seed for the pseudorandom number generator.

    Returns
    -------
    img : `LayeredImage`
        The fake image.

    Raises
    ------
    Raises ``ValueError`` if any of the parameters are invalid.    
    """
    if width <= 0 or height <= 0:
        raise ValueError(f"Invalid dimensions width={width}, height={height}")
    if noise_stdev < 0 or pixel_variance < 0:
        raise ValueError(f"Invalid noise parameters.")

    # Use a set seed if needed.
    if seed is None or seed == -1:
        seed = int.from_bytes(os.urandom(4), "big")
    rng = np.random.default_rng(seed)

    # Create the LayeredImage directly from the layers.
    img = LayeredImage(
        rng.normal(0.0, noise_stdev, (height, width)).astype(np.float32),
        np.full((height, width), pixel_variance).astype(np.float32),
        np.zeros((height, width)).astype(np.float32),
        psf,
        obstime,
    )
    return img


def add_fake_object(img, x, y, flux, psf=None):
    """Add a fake object to a LayeredImage or RawImage

    Parameters
    ----------
    img : `RawImage` or `LayeredImage`
        The image to modify.
    x : `float`
        The x pixel location of the fake object.
    y : `float`
        The y pixel location of the fake object.
    flux : `float`
        The flux value.
    psf : `PSF`
        The PSF for the image.
    """
    if type(img) is LayeredImage:
        sci = img.get_science()
    else:
        sci = img

    if psf is None:
        sci.interpolated_add(x, y, flux)
    else:
        dim = psf.get_dim()
        initial_x = x - psf.get_radius()
        initial_y = y - psf.get_radius()
        for i in range(dim):
            for j in range(dim):
                sci.interpolated_add(float(initial_x + i), float(initial_y + j), flux * psf.get_value(i, j))


def create_fake_times(num_times, t0=0.0, obs_per_day=1, intra_night_gap=0.01, inter_night_gap=1):
    """Create a list of times based on a cluster of ``obs_per_day`` observations
    each night spaced out ``intra_night_gap`` within a night and ``inter_night_gap`` data between
    observing nights.

    Parameters
    ----------
    num_times : `int`
        The number of time steps (number of images).
    t0 : `float`
        The time stamp of the first observation [default=0.0]
    obs_per_day : `int`
        The number of observations on the same night [default=1]
    intra_night_gap : `float`
        The time (in days) between observations in the same night [default=0.01]
    inter_night_gap : `int`
        The number of days between observing runs [default=1]

    Returns
    -------
    result_times : `list`
        A list of ``num_times`` floats indicating the different time stamps.
    """
    if num_times <= 0:
        raise ValueError(f"Invalid number of times {num_times}")

    result_times = []
    seen_on_day = 0  # Number seen so far on the current day
    day_num = 0
    for i in range(num_times):
        result_times.append(t0 + day_num + seen_on_day * intra_night_gap)

        seen_on_day += 1
        if seen_on_day == obs_per_day:
            seen_on_day = 0
            day_num += inter_night_gap
    return result_times


class FakeDataSet:
    """This class creates fake data sets for testing and demo notebooks."""

    def __init__(self, width, height, times, noise_level=2.0, psf_val=0.5, use_seed=False):
        """The constructor.

        Parameters
        ----------
        width : `int`
            The width of the images in pixels.
        height : `int`
            The height of the images in pixels.
        times : `list`
            A list of time stamps, such as produced by ``create_fake_times``.
        noise_level : `float`
            The level of the background noise.
        psf_val : `float`
            The value of the default PSF.
        use_seed : `bool`
            Use a deterministic seed to avoid flaky tests.
        """
        self.width = width
        self.height = height
        self.psf_val = psf_val
        self.noise_level = noise_level
        self.num_times = len(times)
        self.use_seed = use_seed
        self.trajectories = []
        self.times = times
        self.fake_wcs = None

        # Make the image stack.
        self.stack = self.make_fake_ImageStack()

    def set_wcs(self, new_wcs):
        """Set a new fake WCS to be used for this data.

        Parameters
        ----------
        new_wcs : `astropy.wcs.WCS`
            The WCS to use.
        """
        self.fake_wcs = new_wcs

    def make_fake_ImageStack(self):
        """Make a stack of fake layered images.

        Returns
        -------
        stack : `ImageStack`
        """
        p = PSF(self.psf_val)
        stack = ImageStack()
        for i in range(self.num_times):
            img = make_fake_layered_image(
                self.width,
                self.height,
                self.noise_level,
                self.noise_level**2,
                self.times[i],
                p,
                i if self.use_seed else -1,
            )
            stack.append_image(img, force_move=True)
        return stack

    def insert_object(self, trj):
        """Insert a fake object given the trajectory.

        Parameters
        ----------
        trj : `Trajectory`
            The trajectory of the fake object to insert.
        """
        t0 = self.times[0]

        for i in range(self.num_times):
            dt = self.times[i] - t0
            px = trj.get_x_pos(dt)
            py = trj.get_y_pos(dt)

            # Get the image for the timestep, add the object, and
            # re-set the image. This last step needs to be done
            # explicitly because of how pybind handles references.
            current = self.stack.get_single_image(i)
            add_fake_object(current, px, py, trj.flux, current.get_psf())

        # Save the trajectory into the internal list.
        self.trajectories.append(trj)

    def insert_random_object(self, flux):
        """Create a fake object and insert it into the image.

        Parameters
        ----------
        flux : `float`
            The flux of the object.

        Returns
        -------
        t : `Trajectory`
            The trajectory of the inserted object.
        """
        dt = self.times[-1] - self.times[0]

        # Create the random trajectory.
        t = Trajectory()
        t.x = int(random.random() * self.width)
        xe = int(random.random() * self.width)
        t.vx = (xe - t.x) / dt
        t.y = int(random.random() * self.height)
        ye = int(random.random() * self.height)
        t.vy = (ye - t.y) / dt
        t.flux = flux

        # Insert the object.
        self.insert_object(t)

        return t

    def save_fake_data_to_dir(self, data_dir):
        """Create the fake data in a given directory.

        Parameters
        ----------
        data_dir : `str`
            The path of the directory for the fake data.
        """
        # Make the subdirectory if needed.
        dir_path = Path(data_dir)
        if not dir_path.is_dir():
            logger.info(f"Directory {data_dir} does not exist. Creating.")
            os.mkdir(data_dir)

        # Save each of the image files.
        for i in range(self.stack.img_count()):
            img = self.stack.get_single_image(i)
            filename = os.path.join(data_dir, ("%06i.fits" % i))

            # If the file already exists, delete it.
            if Path(filename).exists():
                os.remove(filename)

            # Save the file.
            save_deccam_layered_image(img, filename, wcs=self.fake_wcs)

    def save_time_file(self, file_name):
        """Save the mapping of visit ID -> timestamp to a file.

        Parameters
        ----------
        file_name : `str`
            The file name for the timestamp file.
        """
        mapping = {}
        for i in range(self.num_times):
            id_str = "%06i" % i
            mapping[id_str] = self.times[i]
        FileUtils.save_time_dictionary(file_name, mapping)

    def delete_fake_data_dir(self, data_dir):
        """Remove the fake data in a given directory.

        Parameters
        ----------
        data_dir : `str`
            The path of the directory for the fake data.
        """
        for i in range(self.stack.img_count()):
            img = self.stack.get_single_image(i)
            filename = os.path.join(data_dir, ("%06i.fits" % i))
            if Path(filename).exists():
                os.remove(filename)

    def save_fake_data_to_work_unit(self, filename, config=None):
        """Create the fake data in a WorkUnit file.

        Parameters
        ----------
        filename : `str`
            The name of the resulting WorkUnit file.
        config : `SearchConfiguration`, optional
            The configuration parameters to use. If None then uses
            default values.
        """
        if config is None:
            config = SearchConfiguration()
        work = WorkUnit(im_stack=self.stack, config=config, wcs=self.fake_wcs)
        work.to_fits(filename)
