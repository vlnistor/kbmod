#include "KBMOSearch.h"

namespace search {

#ifdef HAVE_CUDA
  extern "C" void deviceSearchFilter(int num_images, int width, int height, float* psi_vect, float* phi_vect,
                                     PerImageData img_data, SearchParameters params, int num_trajectories,
                                     trajectory* trj_to_search, int num_results, trajectory* best_results);

  void deviceGetCoadds(ImageStack& stack, PerImageData image_data, int num_trajectories,
                       trajectory* trajectories, StampParameters params,
                       std::vector<std::vector<bool> >& use_index_vect, float* results);
#endif

  KBMOSearch::KBMOSearch(ImageStack& imstack) : stack(imstack) {
    max_result_count = 100000;
    debug_info = false;
    psi_phi_generated = false;

    // Default the thresholds.
    params.min_observations = 0;
    params.min_lh = 0.0;

    // Default filtering arguments.
    params.do_sigmag_filter = false;
    params.sgl_L = 0.25;
    params.sgl_H = 0.75;
    params.sigmag_coeff = -1.0;

    // Default the encoding parameters.
    params.psi_num_bytes = -1;
    params.phi_num_bytes = -1;

    // Default pixel starting bounds.
    params.x_start_min = 0;
    params.x_start_max = stack.get_width();
    params.y_start_min = 0;
    params.y_start_max = stack.getHeight();

    // Set default values for the barycentric correction.
    bary_corrs = std::vector<BaryCorrection>(stack.img_count());
    params.use_corr = false;
    use_corr = false;

    params.debug = false;
  }

  void KBMOSearch::set_debug(bool d) {
    debug_info = d;
    params.debug = d;
  }

  void KBMOSearch::enable_corr(std::vector<float> bary_corr_coeff) {
    use_corr = true;
    params.use_corr = true;
    for (int i = 0; i < stack.img_count(); i++) {
      int j = i * 6;
      bary_corrs[i].dx = bary_corr_coeff[j];
      bary_corrs[i].dxdx = bary_corr_coeff[j + 1];
      bary_corrs[i].dxdy = bary_corr_coeff[j + 2];
      bary_corrs[i].dy = bary_corr_coeff[j + 3];
      bary_corrs[i].dydx = bary_corr_coeff[j + 4];
      bary_corrs[i].dydy = bary_corr_coeff[j + 5];
    }
  }

  void KBMOSearch::enable_gpu_sigmag_filter(std::vector<float> percentiles, float sigmag_coeff,
                                            float min_lh) {
    params.do_sigmag_filter = true;
    params.sgl_L = percentiles[0];
    params.sgl_H = percentiles[1];
    params.sigmag_coeff = sigmag_coeff;
    params.min_lh = min_lh;
  }

  void KBMOSearch::enable_gpu_encoding(int psi_num_bytes, int phi_num_bytes) {
    // Make sure the encoding is one of the supported options.
    // Otherwise use default float (aka no encoding).
    if (psi_num_bytes == 1 || psi_num_bytes == 2) {
      params.psi_num_bytes = psi_num_bytes;
    } else {
      params.psi_num_bytes = -1;
    }
    if (phi_num_bytes == 1 || phi_num_bytes == 2) {
      params.phi_num_bytes = phi_num_bytes;
    } else {
      params.phi_num_bytes = -1;
    }
  }

  void KBMOSearch::set_start_bounds_x(int x_min, int x_max) {
    params.x_start_min = x_min;
    params.x_start_max = x_max;
  }

  void KBMOSearch::set_start_bounds_y(int y_min, int y_max) {
    params.y_start_min = y_min;
    params.y_start_max = y_max;
  }

  void KBMOSearch::search(int ang_steps, int vel_steps, float min_ang, float max_ang, float min_vel,
                          float max_vel, int min_observations) {
    preparePsiPhi();
    createSearchList(ang_steps, vel_steps, min_ang, max_ang, min_vel, max_vel);

    startTimer("Creating psi/phi buffers");
    std::vector<float> psi_vect;
    std::vector<float> phi_vect;
    fillPsiAndphi_vects(psi_images, phi_images, &psi_vect, &phi_vect);
    endTimer();

    // Create a data stucture for the per-image data.
    PerImageData img_data;
    img_data.num_images = stack.img_count();
    img_data.image_times = stack.get_timesDataRef();
    if (params.use_corr) img_data.bary_corrs = &bary_corrs[0];

    // Compute the encoding parameters for psi and phi if needed.
    // Vectors need to be created outside the if so they stay in scope.
    std::vector<scaleParameters> psi_scale_vect;
    std::vector<scaleParameters> phi_scale_vect;
    if (params.psi_num_bytes > 0) {
      psi_scale_vect = computeImageScaling(psi_images, params.psi_num_bytes);
      img_data.psi_params = psi_scale_vect.data();
    }
    if (params.phi_num_bytes > 0) {
      phi_scale_vect = computeImageScaling(phi_images, params.phi_num_bytes);
      img_data.phi_params = phi_scale_vect.data();
    }

    // Allocate a vector for the results.
    int num_search_pixels =
      ((params.x_start_max - params.x_start_min) * (params.y_start_max - params.y_start_min));
    int max_results = num_search_pixels * RESULTS_PER_PIXEL;
    if (debug_info) {
      std::cout << "Searching X=[" << params.x_start_min << ", " << params.x_start_max << "]"
                << " Y=[" << params.y_start_min << ", " << params.y_start_max << "]\n";
      std::cout << "Allocating space for " << max_results << " results.\n";
    }
    results = std::vector<trajectory>(max_results);
    if (debug_info) std::cout << search_list.size() << " trajectories... \n" << std::flush;

    // Set the minimum number of observations.
    params.min_observations = min_observations;

    // Do the actual search on the GPU.
    startTimer("Searching");
#ifdef HAVE_CUDA
    deviceSearchFilter(stack.img_count(), stack.get_width(), stack.getHeight(), psi_vect.data(), phi_vect.data(),
                       img_data, params, search_list.size(), search_list.data(), max_results, results.data());
#else
    throw std::runtime_error("Non-GPU search is not implemented.");
#endif
    endTimer();

    startTimer("Sorting results");
    sortResults();
    endTimer();
  }

  void KBMOSearch::save_psiphi(const std::string& path) {
    preparePsiPhi();
    save_images(path);
  }

  void KBMOSearch::preparePsiPhi() {
    if (!psi_phi_generated) {
      psi_images.clear();
      phi_images.clear();

      // Compute Phi and Psi from convolved images
      // while leaving masked pixels alone
      // Reinsert 0s for NO_DATA?
      const int num_images = stack.img_count();
      for (int i = 0; i < num_images; ++i) {
        LayeredImage& img = stack.get_single_image(i);
        psi_images.push_back(img.generate_psi_image());
        phi_images.push_back(img.generate_phi_image());
      }

      psi_phi_generated = true;
    }
  }

  std::vector<scaleParameters> KBMOSearch::computeImageScaling(const std::vector<RawImage>& vect,
                                                               int encoding_bytes) const {
    std::vector<scaleParameters> result;

    const int num_images = vect.size();
    for (int i = 0; i < num_images; ++i) {
      scaleParameters params;
      params.scale = 1.0;

      std::array<float, 2> bnds = vect[i].compute_bounds();
      params.min_val = bnds[0];
      params.max_val = bnds[1];

      // Increase width to avoid divide by zero.
      float width = (params.max_val - params.min_val);
      if (width < 1e-6) width = 1e-6;

      // Set the scale if we are encoding the values.
      if (encoding_bytes == 1 || encoding_bytes == 2) {
        long int num_values = (1 << (8 * encoding_bytes)) - 1;
        params.scale = width / (double)num_values;
      }

      result.push_back(params);
    }

    return result;
  }

  void KBMOSearch::save_images(const std::string& path) {
    for (int i = 0; i < stack.img_count(); ++i) {
      std::string number = std::to_string(i);
      // Add leading zeros
      number = std::string(4 - number.length(), '0') + number;
      psi_images[i].save_to_file(path + "/psi/PSI" + number + ".fits");
      phi_images[i].save_to_file(path + "/phi/PHI" + number + ".fits");
    }
  }

  void KBMOSearch::createSearchList(int angle_steps, int velocity_steps, float min_ang, float max_ang,
                                    float min_vel, float max_vel) {
    std::vector<float> angles(angle_steps);
    float ang_stepsize = (max_ang - min_ang) / float(angle_steps);
    for (int i = 0; i < angle_steps; ++i) {
      angles[i] = min_ang + float(i) * ang_stepsize;
    }

    std::vector<float> velocities(velocity_steps);
    float vel_stepsize = (max_vel - min_vel) / float(velocity_steps);
    for (int i = 0; i < velocity_steps; ++i) {
      velocities[i] = min_vel + float(i) * vel_stepsize;
    }

    int trajCount = angle_steps * velocity_steps;
    search_list = std::vector<trajectory>(trajCount);
    for (int a = 0; a < angle_steps; ++a) {
      for (int v = 0; v < velocity_steps; ++v) {
        search_list[a * velocity_steps + v].x_vel = cos(angles[a]) * velocities[v];
        search_list[a * velocity_steps + v].y_vel = sin(angles[a]) * velocities[v];
      }
    }
  }

  void KBMOSearch::fillPsiAndphi_vects(const std::vector<RawImage>& psi_imgs,
                                       const std::vector<RawImage>& phi_imgs, std::vector<float>* psi_vect,
                                       std::vector<float>* phi_vect) {
    assert(psi_vect != NULL);
    assert(phi_vect != NULL);

    int num_images = psi_imgs.size();
    assert(num_images > 0);
    assert(phi_imgs.size() == num_images);

    int num_pixels = psi_imgs[0].get_npixels();
    for (int i = 0; i < num_images; ++i) {
      assert(psi_imgs[i].get_npixels() == num_pixels);
      assert(phi_imgs[i].get_npixels() == num_pixels);
    }

    psi_vect->clear();
    psi_vect->reserve(num_images * num_pixels);
    phi_vect->clear();
    phi_vect->reserve(num_images * num_pixels);

    for (int i = 0; i < num_images; ++i) {
      const std::vector<float>& psi_ref = psi_imgs[i].get_pixels();
      const std::vector<float>& phi_ref = phi_imgs[i].get_pixels();
      for (unsigned p = 0; p < num_pixels; ++p) {
        psi_vect->push_back(psi_ref[p]);
        phi_vect->push_back(phi_ref[p]);
      }
    }
  }

  std::vector<RawImage> KBMOSearch::scienceStamps(const trajectory& trj, int radius, bool interpolate,
                                                  bool keep_no_data, const std::vector<bool>& use_index) {
    if (use_index.size() > 0 && use_index.size() != stack.img_count()) {
      throw std::runtime_error("Wrong size use_index passed into scienceStamps()");
    }
    bool use_all_stamps = use_index.size() == 0;

    std::vector<RawImage> stamps;
    int num_times = stack.img_count();
    for (int i = 0; i < num_times; ++i) {
      if (use_all_stamps || use_index[i]) {
        PixelPos pos = get_trajectory_position(trj, i);
        RawImage& img = stack.get_single_image(i).get_science();
        stamps.push_back(img.create_stamp(pos.x, pos.y, radius, interpolate, keep_no_data));
      }
    }
    return stamps;
  }

  // For stamps used for visualization we interpolate the pixel values, replace
  // NO_DATA tages with zeros, and return all the stamps (regardless of whether
  // individual timesteps have been filtered).
  std::vector<RawImage> KBMOSearch::science_stamps_for_viz(const trajectory& t, int radius) {
    std::vector<bool> empty_vect;
    return scienceStamps(t, radius, true /*=interpolate*/, false /*=keep_no_data*/, empty_vect);
  }

  // For creating coadded stamps, we do not interpolate the pixel values and keep
  // NO_DATA tagged (so we can filter it out of mean/median).
  RawImage KBMOSearch::median_science_stamp(const trajectory& trj, int radius,
                                            const std::vector<bool>& use_index) {
    return create_median_image(
                               scienceStamps(trj, radius, false /*=interpolate*/, true /*=keep_no_data*/, use_index));
  }

  // For creating coadded stamps, we do not interpolate the pixel values and keep
  // NO_DATA tagged (so we can filter it out of mean/median).
  RawImage KBMOSearch::mean_science_stamp(const trajectory& trj, int radius, const std::vector<bool>& use_index) {
    return create_mean_image(
                             scienceStamps(trj, radius, false /*=interpolate*/, true /*=keep_no_data*/, use_index));
  }

  // For creating summed stamps, we do not interpolate the pixel values and replace NO_DATA
  // with zero (which is the same as filtering it out for the sum).
  RawImage KBMOSearch::summed_science_stamp(const trajectory& trj, int radius,
                                            const std::vector<bool>& use_index) {
    return create_summed_image(
                               scienceStamps(trj, radius, false /*=interpolate*/, false /*=keep_no_data*/, use_index));
  }

  bool KBMOSearch::filter_stamp(const RawImage& img, const StampParameters& params) {
    // Allocate space for the coadd information and initialize to zero.
    const int stamp_width = 2 * params.radius + 1;
    const int stamp_ppi = stamp_width * stamp_width;
    const std::vector<float>& pixels = img.get_pixels();

    // Filter on the peak's position.
    PixelPos pos = img.find_peak(true);
    if ((abs(pos.x - params.radius) >= params.peak_offset_x) ||
        (abs(pos.y - params.radius) >= params.peak_offset_y)) {
      return true;
    }

    // Filter on the percentage of flux in the central pixel.
    if (params.center_thresh > 0.0) {
      const std::vector<float>& pixels = img.get_pixels();
      float center_val = pixels[(int)pos.y * stamp_width + (int)pos.x];
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

  std::vector<RawImage> KBMOSearch::coadded_science_stamps(std::vector<trajectory>& t_array,
                                                           std::vector<std::vector<bool> >& use_index_vect,
                                                           const StampParameters& params, bool use_gpu) {
    if (use_gpu) {
#ifdef HAVE_CUDA
      return coadded_science_stampsGPU(t_array, use_index_vect, params);
#else
      std::cout << "WARNING: GPU is not enabled. Performing co-adds on the CPU.";
      //py::print("WARNING: GPU is not enabled. Performing co-adds on the CPU.");
#endif
    }
    return coadded_science_stampsCPU(t_array, use_index_vect, params);
  }

  std::vector<RawImage> KBMOSearch::coadded_science_stampsCPU(std::vector<trajectory>& t_array,
                                                              std::vector<std::vector<bool> >& use_index_vect,
                                                              const StampParameters& params) {
    const int num_trajectories = t_array.size();
    std::vector<RawImage> results(num_trajectories);
    std::vector<float> empty_pixels(1, NO_DATA);

    for (int i = 0; i < num_trajectories; ++i) {
      std::vector<RawImage> stamps =
        scienceStamps(t_array[i], params.radius, false, true, use_index_vect[i]);

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
        results[i] = RawImage(1, 1, empty_pixels);
      } else {
        results[i] = coadd;
      }
    }

    return results;
  }

  std::vector<RawImage> KBMOSearch::coadded_science_stampsGPU(std::vector<trajectory>& t_array,
                                                              std::vector<std::vector<bool> >& use_index_vect,
                                                              const StampParameters& params) {
    // Right now only limited stamp sizes are allowed.
    if (2 * params.radius + 1 > MAX_STAMP_EDGE || params.radius <= 0) {
      throw std::runtime_error("Invalid Radius.");
    }

    const int num_images = stack.img_count();
    const int width = stack.get_width();
    const int height = stack.getHeight();

    // Create a data stucture for the per-image data.
    PerImageData img_data;
    img_data.num_images = num_images;
    img_data.image_times = stack.get_timesDataRef();

    // Allocate space for the results.
    const int num_trajectories = t_array.size();
    const int stamp_width = 2 * params.radius + 1;
    const int stamp_ppi = stamp_width * stamp_width;
    std::vector<float> stamp_data(stamp_ppi * num_trajectories);

    // Do the co-adds.
#ifdef HAVE_CUDA
    deviceGetCoadds(stack, img_data, num_trajectories, t_array.data(), params, use_index_vect,
                    stamp_data.data());
#else
    throw std::runtime_error("Non-GPU co-adds is not implemented.");
#endif

    // Copy the stamps into RawImages and do the filtering.
    std::vector<RawImage> results(num_trajectories);
    std::vector<float> current_pixels(stamp_ppi, 0.0);
    std::vector<float> empty_pixels(1, NO_DATA);
    for (int t = 0; t < num_trajectories; ++t) {
      // Copy the data into a single RawImage.
      int offset = t * stamp_ppi;
      for (unsigned p = 0; p < stamp_ppi; ++p) {
        current_pixels[p] = stamp_data[offset + p];
      }
      RawImage current_image = RawImage(stamp_width, stamp_width, current_pixels);

      if (params.do_filtering && filter_stamp(current_image, params)) {
        results[t] = RawImage(1, 1, empty_pixels);
      } else {
        results[t] = RawImage(stamp_width, stamp_width, current_pixels);
      }
    }
    return results;
  }

  std::vector<RawImage> KBMOSearch::create_stamps(trajectory t, int radius, const std::vector<RawImage*>& imgs,
                                                  bool interpolate) {
    if (radius < 0) throw std::runtime_error("stamp radius must be at least 0");
    std::vector<RawImage> stamps;
    for (int i = 0; i < imgs.size(); ++i) {
      PixelPos pos = get_trajectory_position(t, i);
      stamps.push_back(imgs[i]->create_stamp(pos.x, pos.y, radius, interpolate, false));
    }
    return stamps;
  }

  PixelPos KBMOSearch::get_trajectory_position(const trajectory& t, int i) const {
    float time = stack.get_times()[i];
    if (use_corr) {
      return {t.x + time * t.x_vel + bary_corrs[i].dx + t.x * bary_corrs[i].dxdx + t.y * bary_corrs[i].dxdy,
        t.y + time * t.y_vel + bary_corrs[i].dy + t.x * bary_corrs[i].dydx +
        t.y * bary_corrs[i].dydy};
    } else {
      return {t.x + time * t.x_vel, t.y + time * t.y_vel};
    }
  }

  std::vector<PixelPos> KBMOSearch::get_trajectory_positions(trajectory& t) const {
    std::vector<PixelPos> results;
    int num_times = stack.img_count();
    for (int i = 0; i < num_times; ++i) {
      PixelPos pos = get_trajectory_position(t, i);
      results.push_back(pos);
    }
    return results;
  }

  std::vector<float> KBMOSearch::createCurves(trajectory t, const std::vector<RawImage>& imgs) {
    /*Create a lightcurve from an image along a trajectory
     *
     *  INPUT-
     *    trajectory t - The trajectory along which to compute the lightcurve
     *    std::vector<RawImage*> imgs - The image from which to compute the
     *      trajectory. Most likely a psiImage or a phiImage.
     *  Output-
     *    std::vector<float> lightcurve - The computed trajectory
     */

    int img_size = imgs.size();
    std::vector<float> lightcurve;
    lightcurve.reserve(img_size);
    const std::vector<float>& times = stack.get_times();
    for (int i = 0; i < img_size; ++i) {
      /* Do not use get_pixel_interp(), because results from createCurves must
       * be able to recover the same likelihoods as the ones reported by the
       * gpu search.*/
      float pix_val;
      if (use_corr) {
        PixelPos pos = get_trajectory_position(t, i);
        pix_val = imgs[i].get_pixel(int(pos.x + 0.5), int(pos.y + 0.5));
      }
      /* Does not use get_trajectory_position to be backwards compatible with Hits_Rerun */
      else {
        pix_val = imgs[i].get_pixel(t.x + int(times[i] * t.x_vel + 0.5),
                                    t.y + int(times[i] * t.y_vel + 0.5));
      }
      if (pix_val == NO_DATA) pix_val = 0.0;
      lightcurve.push_back(pix_val);
    }
    return lightcurve;
  }

  std::vector<float> KBMOSearch::psi_curves(trajectory& t) {
    /*Generate a psi lightcurve for further analysis
     *  INPUT-
     *    trajectory& t - The trajectory along which to find the lightcurve
     *  OUTPUT-
     *    std::vector<float> - A vector of the lightcurve values
     */
    preparePsiPhi();
    return createCurves(t, psi_images);
  }

  std::vector<float> KBMOSearch::phi_curves(trajectory& t) {
    /*Generate a phi lightcurve for further analysis
     *  INPUT-
     *    trajectory& t - The trajectory along which to find the lightcurve
     *  OUTPUT-
     *    std::vector<float> - A vector of the lightcurve values
     */
    preparePsiPhi();
    return createCurves(t, phi_images);
  }

  std::vector<RawImage>& KBMOSearch::getPsiImages() { return psi_images; }

  std::vector<RawImage>& KBMOSearch::getPhiImages() { return phi_images; }

  void KBMOSearch::sortResults() {
    __gnu_parallel::sort(results.begin(), results.end(),
                         [](trajectory a, trajectory b) { return b.lh < a.lh; });
  }

  void KBMOSearch::filter_results(int min_observations) {
    results.erase(std::remove_if(results.begin(), results.end(),
                                 std::bind([](trajectory t, int cutoff) { return t.obs_count < cutoff; },
                                           std::placeholders::_1, min_observations)),
                  results.end());
  }

  void KBMOSearch::filter_resultsLH(float min_lh) {
    results.erase(std::remove_if(results.begin(), results.end(),
                                 std::bind([](trajectory t, float cutoff) { return t.lh < cutoff; },
                                           std::placeholders::_1, min_lh)),
                  results.end());
  }

  std::vector<trajectory> KBMOSearch::get_results(int start, int count) {
    if (start + count >= results.size()) {
      count = results.size() - start;
    }
    if (start < 0) throw std::runtime_error("start must be 0 or greater");
    return std::vector<trajectory>(results.begin() + start, results.begin() + start + count);
  }

  // This function is used only for testing by injecting known result trajectories.
  void KBMOSearch::set_results(const std::vector<trajectory>& new_results) { results = new_results; }

  void KBMOSearch::startTimer(const std::string& message) {
    if (debug_info) {
      std::cout << message << "... " << std::flush;
      t_start = std::chrono::system_clock::now();
    }
  }

  void KBMOSearch::endTimer() {
    if (debug_info) {
      t_end = std::chrono::system_clock::now();
      t_delta = t_end - t_start;
      std::cout << " Took " << t_delta.count() << " seconds.\n" << std::flush;
    }
  }

#ifdef Py_PYTHON_H
  static void stack_search_bindings(py::module &m) {
    using tj = search::trajectory;
    using pf = search::PSF;
    using ri = search::RawImage;
    using is = search::ImageStack;
    using ks = search::KBMOSearch;

    py::class_<ks>(m, "StackSearch", pydocs::DOC_StackSearch)
      .def(py::init<is &>())
      .def("save_psi_phi", &ks::save_psiphi, pydocs::DOC_StackSearch_save_psi_phi)
      .def("search", &ks::search, pydocs::DOC_StackSearch_search)
      .def("enable_gpu_sigmag_filter", &ks::enable_gpu_sigmag_filter, pydocs::DOC_StackSearch_enable_gpu_sigmag_filter)
      .def("enable_gpu_encoding", &ks::enable_gpu_encoding, pydocs::DOC_StackSearch_enable_gpu_encoding)
      .def("enable_corr", &ks::enable_corr, pydocs::DOC_StackSearch_enable_corr)
      .def("set_start_bounds_x", &ks::set_start_bounds_x, pydocs::DOC_StackSearch_set_start_bounds_x)
      .def("set_start_bounds_y", &ks::set_start_bounds_y, pydocs::DOC_StackSearch_set_start_bounds_y)
      .def("set_debug", &ks::set_debug, pydocs::DOC_StackSearch_set_debug)
      .def("filter_min_obs", &ks::filter_results, pydocs::DOC_StackSearch_filter_min_obs)
      .def("get_num_images", &ks::num_images, pydocs::DOC_StackSearch_get_num_images)
      .def("get_image_stack", &ks::get_imagestack, pydocs::DOC_StackSearch_get_image_stack)
      // Science Stamp Functions
      .def("science_viz_stamps", &ks::science_stamps_for_viz, pydocs::DOC_StackSearch_science_viz_stamps)
      .def("median_sci_stamp", &ks::median_science_stamp, pydocs::DOC_StackSearch_median_sci_stamp)
      .def("mean_sci_stamp", &ks::mean_science_stamp, pydocs::DOC_StackSearch_mean_sci_stamp)
      .def("summed_sci_stamp", &ks::summed_science_stamp, pydocs::DOC_StackSearch_summed_sci_stamp)
      .def("coadded_stamps", //wth is happening here
           (std::vector<ri>(ks::*)(std::vector<tj> &, std::vector<std::vector<bool>> &,
                                   const search::StampParameters &, bool)) &
           ks::coadded_science_stamps, pydocs::DOC_StackSearch_coadded_stamps)
      // For testing
      .def("filter_stamp", &ks::filter_stamp, pydocs::DOC_StackSearch_filter_stamp)
      .def("get_traj_pos", &ks::get_trajectory_position, pydocs::DOC_StackSearch_get_traj_pos)
      .def("get_mult_traj_pos", &ks::get_trajectory_positions, pydocs::DOC_StackSearch_get_mult_traj_pos)
      .def("psi_curves", (std::vector<float>(ks::*)(tj &)) & ks::psi_curves, pydocs::DOC_StackSearch_psi_curves)
      .def("phi_curves", (std::vector<float>(ks::*)(tj &)) & ks::phi_curves, pydocs::DOC_StackSearch_phi_curves)
      .def("prepare_psi_phi", &ks::preparePsiPhi, pydocs::DOC_StackSearch_prepare_psi_phi)
      .def("get_psi_images", &ks::getPsiImages, pydocs::DOC_StackSearch_get_psi_images)
      .def("get_phi_images", &ks::getPhiImages, pydocs::DOC_StackSearch_get_phi_images)
      .def("get_results", &ks::get_results, pydocs::DOC_StackSearch_get_results)
      .def("set_results", &ks::set_results, pydocs::DOC_StackSearch_set_results);
  }
#endif /* Py_PYTHON_H */
} /* namespace search */
