#include "image_stack.h"

namespace search {

ImageStack::ImageStack(const std::vector<LayeredImage>& imgs) { images = imgs; }

LayeredImage& ImageStack::get_single_image(int index) {
    if (index < 0 || index > images.size()) throw std::out_of_range("ImageStack index out of bounds.");
    return images[index];
}

float ImageStack::get_obstime(int index) const {
    if (index < 0 || index > images.size()) throw std::out_of_range("ImageStack index out of bounds.");
    return images[index].get_obstime();
}

float ImageStack::get_zeroed_time(int index) const {
    if (index < 0 || index > images.size()) throw std::out_of_range("ImageStack index out of bounds.");
    return images[index].get_obstime() - images[0].get_obstime();
}

std::vector<float> ImageStack::build_zeroed_times() const {
    std::vector<float> zeroed_times = std::vector<float>();
    if (images.size() > 0) {
        double t0 = images[0].get_obstime();
        for (auto& i : images) {
            zeroed_times.push_back(i.get_obstime() - t0);
        }
    }
    return zeroed_times;
}

void ImageStack::convolve_psf() {
    for (auto& i : images) i.convolve_psf();
}

RawImage ImageStack::make_global_mask(int flags, int threshold) {
    int npixels = get_npixels();

    // Start with an empty global mask.
    RawImage global_mask = RawImage(get_width(), get_height());
    global_mask.set_all(0.0);

    // For each pixel count the number of images where it is masked.
    std::vector<int> counts(npixels, 0);
    for (unsigned int img = 0; img < images.size(); ++img) {
        auto imgMask = images[img].get_mask().get_image().reshaped();

        // Count the number of times a pixel has any of the given flags
        for (unsigned int pixel = 0; pixel < npixels; ++pixel) {
            if ((flags & static_cast<int>(imgMask[pixel])) != 0) counts[pixel]++;
        }
    }

    // Set all pixels below threshold to 0 and all above to 1
    auto global_m = global_mask.get_image().reshaped();
    for (unsigned int p = 0; p < npixels; ++p) {
        global_m[p] = counts[p] < threshold ? 0.0 : 1.0;
    }

    return global_mask;
}

#ifdef Py_PYTHON_H
static void image_stack_bindings(py::module& m) {
    using is = search::ImageStack;
    using li = search::LayeredImage;
    using pf = search::PSF;

    py::class_<is>(m, "ImageStack", pydocs::DOC_ImageStack)
            .def(py::init<std::vector<li>>())
            .def("get_images", &is::get_images, pydocs::DOC_ImageStack_get_images)
            .def("get_single_image", &is::get_single_image, py::return_value_policy::reference_internal,
                 pydocs::DOC_ImageStack_get_single_image)
            .def("get_obstime", &is::get_obstime, pydocs::DOC_ImageStack_get_obstime)
            .def("get_zeroed_time", &is::get_zeroed_time, pydocs::DOC_ImageStack_get_zeroed_time)
            .def("build_zeroed_times", &is::build_zeroed_times, pydocs::DOC_ImageStack_build_zeroed_times)
            .def("img_count", &is::img_count, pydocs::DOC_ImageStack_img_count)
            .def("make_global_mask", &is::make_global_mask, pydocs::DOC_ImageStack_make_global_mask)
            .def("convolve_psf", &is::convolve_psf, pydocs::DOC_ImageStack_convolve_psf)
            .def("get_width", &is::get_width, pydocs::DOC_ImageStack_get_width)
            .def("get_height", &is::get_height, pydocs::DOC_ImageStack_get_height)
            .def("get_npixels", &is::get_npixels, pydocs::DOC_ImageStack_get_npixels);
}

#endif /* Py_PYTHON_H */

} /* namespace search */
