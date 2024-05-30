#include "stamp_creator.h"

namespace search {
#ifdef HAVE_CUDA
void deviceGetCoadds(const unsigned int num_images, const unsigned int width, const unsigned int height,
                     GPUArray<float>& image_data, GPUArray<double>& image_times,
                     std::vector<Trajectory>& trajectories, StampParameters params,
                     std::vector<std::vector<bool>>& use_index_vect, std::vector<float> &results);
#endif

StampCreator::StampCreator() {}

std::vector<RawImage> StampCreator::create_stamps(ImageStack& stack, const Trajectory& trj, int radius,
                                                  bool keep_no_data, const std::vector<bool>& use_index) {
    if (use_index.size() > 0 && use_index.size() != stack.img_count()) {
        throw std::runtime_error("Wrong size use_index passed into create_stamps()");
    }
    bool use_all_stamps = use_index.size() == 0;

    std::vector<RawImage> stamps;
    int num_times = stack.img_count();
    for (int i = 0; i < num_times; ++i) {
        if (use_all_stamps || use_index[i]) {
            // Calculate the trajectory position.
            double time = stack.get_zeroed_time(i);
            Point pos{trj.get_x_pos(time), trj.get_y_pos(time)};
            RawImage& img = stack.get_single_image(i).get_science();
            stamps.push_back(img.create_stamp(pos, radius, keep_no_data));
        }
    }
    return stamps;
}

// For stamps used for visualization we replace invalid pixels with zeros
// and return all the stamps (regardless of whether individual timesteps
// have been filtered).
std::vector<RawImage> StampCreator::get_stamps(ImageStack& stack, const Trajectory& t, int radius) {
    std::vector<bool> empty_vect;
    return create_stamps(stack, t, radius, false /*=keep_no_data*/, empty_vect);
}

// For creating coadded stamps, we do not interpolate the pixel values and keep
// invalid pixels tagged (so we can filter it out of mean/median).
RawImage StampCreator::get_median_stamp(ImageStack& stack, const Trajectory& trj, int radius,
                                        const std::vector<bool>& use_index) {
    return create_median_image(create_stamps(stack, trj, radius, true /*=keep_no_data*/, use_index));
}

// For creating coadded stamps, we do not interpolate the pixel values and keep
// invalid pixels tagged (so we can filter it out of mean/median).
RawImage StampCreator::get_mean_stamp(ImageStack& stack, const Trajectory& trj, int radius,
                                      const std::vector<bool>& use_index) {
    return create_mean_image(create_stamps(stack, trj, radius, true /*=keep_no_data*/, use_index));
}

// For creating summed stamps, we do not interpolate the pixel values and replace
// invalid pixels with zero (which is the same as filtering it out for the sum).
RawImage StampCreator::get_summed_stamp(ImageStack& stack, const Trajectory& trj, int radius,
                                        const std::vector<bool>& use_index) {
    return create_summed_image(create_stamps(stack, trj, radius, false /*=keep_no_data*/, use_index));
}

std::vector<RawImage> StampCreator::get_coadded_stamps(ImageStack& stack, std::vector<Trajectory>& t_array,
                                                       std::vector<std::vector<bool>>& use_index_vect,
                                                       const StampParameters& params, bool use_gpu) {
    if (use_gpu) {
#ifdef HAVE_CUDA
        logging::getLogger("kbmod.search.stamp_creator")->info("Performing co-adds on the GPU.");
        return get_coadded_stamps_gpu(stack, t_array, use_index_vect, params);
#else
        logging::getLogger("kbmod.search.stamp_creator")
                ->warning("GPU is not enabled. Performing co-adds on the CPU.");
#endif
    }
    return get_coadded_stamps_cpu(stack, t_array, use_index_vect, params);
}

std::vector<RawImage> StampCreator::get_coadded_stamps_cpu(ImageStack& stack,
                                                           std::vector<Trajectory>& t_array,
                                                           std::vector<std::vector<bool>>& use_index_vect,
                                                           const StampParameters& params) {
    const int num_trajectories = t_array.size();
    std::vector<RawImage> results(num_trajectories);

    for (int i = 0; i < num_trajectories; ++i) {
        std::vector<RawImage> stamps =
                StampCreator::create_stamps(stack, t_array[i], params.radius, true, use_index_vect[i]);

        RawImage coadd(1, 1);
        switch (params.stamp_type) {
            case STAMP_MEDIAN:
                coadd = create_median_image(stamps);
                break;
            case STAMP_MEAN:
                coadd = create_mean_image(stamps);
                break;
            case STAMP_SUM:
                coadd = create_summed_image(stamps);
                break;
            default:
                throw std::runtime_error("Invalid stamp coadd type.");
        }

        // Do the filtering if needed.
        if (params.do_filtering && filter_stamp(coadd, params)) {
            results[i] = RawImage(1, 1, NO_DATA);
        } else {
            results[i] = coadd;
        }
    }

    return results;
}

bool StampCreator::filter_stamp(const RawImage& img, const StampParameters& params) {
    // Allocate space for the coadd information and initialize to zero.
    const int stamp_width = 2 * params.radius + 1;
    const int stamp_ppi = stamp_width * stamp_width;
    // this ends up being something like eigen::vector1f something, not vector
    // but it behaves in all the same ways so just let it figure it out itself
    const auto& pixels = img.get_image().reshaped();

    // Filter on the peak's position.
    Index idx = img.find_peak(true);
    if ((abs(idx.i - params.radius) >= params.peak_offset_x) ||
        (abs(idx.j - params.radius) >= params.peak_offset_y)) {
        return true;
    }

    // Filter on the percentage of flux in the central pixel.
    if (params.center_thresh > 0.0) {
        const auto& pixels = img.get_image().reshaped();
        float center_val = pixels[idx.j * stamp_width + idx.i];
        float pixel_sum = 0.0;
        for (int p = 0; p < stamp_ppi; ++p) {
            pixel_sum += pixels[p];
        }

        if (center_val / pixel_sum < params.center_thresh) {
            return true;
        }
    }

    // Filter on the image moments.
    ImageMoments moments = img.find_central_moments();
    if ((fabs(moments.m01) >= params.m01_limit) || (fabs(moments.m10) >= params.m10_limit) ||
        (fabs(moments.m11) >= params.m11_limit) || (moments.m02 >= params.m02_limit) ||
        (moments.m20 >= params.m20_limit)) {
        return true;
    }

    return false;
}

std::vector<RawImage> StampCreator::get_coadded_stamps_gpu(ImageStack& stack,
                                                           std::vector<Trajectory>& t_array,
                                                           std::vector<std::vector<bool>>& use_index_vect,
                                                           const StampParameters& params) {
    // Right now only limited stamp sizes are allowed.
    if (2 * params.radius + 1 > MAX_STAMP_EDGE || params.radius <= 0) {
        throw std::runtime_error("Invalid Radius.");
    }

    const int num_images = stack.img_count();
    const int width = stack.get_width();
    const int height = stack.get_height();

    // Allocate space for the results.
    const int num_trajectories = t_array.size();
    const int stamp_width = 2 * params.radius + 1;
    const int stamp_ppi = stamp_width * stamp_width;
    std::vector<float> stamp_data(stamp_ppi * num_trajectories);

    // Do the co-adds.
#ifdef HAVE_CUDA
    bool was_on_gpu = stack.on_gpu();
    if (!was_on_gpu) stack.copy_to_gpu();

    deviceGetCoadds(num_images, width, height, stack.get_gpu_image_array(), stack.get_gpu_time_array(),
                    t_array, params, use_index_vect, stamp_data);

    // If we put the data on GPU this function, make sure to clean it up.
    if (!was_on_gpu) stack.clear_from_gpu();
#else
    throw std::runtime_error("Non-GPU co-adds is not implemented.");
#endif

    // Copy the stamps into RawImages and do the filtering.
    std::vector<RawImage> results(num_trajectories);
    std::vector<float> current_pixels(stamp_ppi, 0.0);
    for (int t = 0; t < num_trajectories; ++t) {
        // Copy the data into a single RawImage.
        int offset = t * stamp_ppi;
        for (unsigned p = 0; p < stamp_ppi; ++p) {
            current_pixels[p] = stamp_data[offset + p];
        }

        Image tmp = Eigen::Map<Image>(current_pixels.data(), stamp_width, stamp_width);
        RawImage current_image = RawImage(tmp);

        if (params.do_filtering && filter_stamp(current_image, params)) {
            results[t] = RawImage(1, 1, NO_DATA);
        } else {
            results[t] = current_image;
        }
    }
    return results;
}

#ifdef Py_PYTHON_H
static void stamp_creator_bindings(py::module& m) {
    using sc = search::StampCreator;

    py::class_<sc>(m, "StampCreator", pydocs::DOC_StampCreator)
            .def(py::init<>())
            .def_static("get_stamps", &sc::get_stamps, pydocs::DOC_StampCreator_get_stamps)
            .def_static("get_median_stamp", &sc::get_median_stamp, pydocs::DOC_StampCreator_get_median_stamp)
            .def_static("get_mean_stamp", &sc::get_mean_stamp, pydocs::DOC_StampCreator_get_mean_stamp)
            .def_static("get_summed_stamp", &sc::get_summed_stamp, pydocs::DOC_StampCreator_get_summed_stamp)
            .def_static("get_coadded_stamps", &sc::get_coadded_stamps,
                        pydocs::DOC_StampCreator_get_coadded_stamps)
            .def_static("filter_stamp", &sc::filter_stamp, pydocs::DOC_StampCreator_filter_stamp);
}
#endif /* Py_PYTHON_H */

} /* namespace search */
