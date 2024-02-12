#ifndef KBMODSEARCH_H_
#define KBMODSEARCH_H_

#include <parallel/algorithm>
#include <algorithm>
#include <functional>
#include <iostream>
#include <fstream>
#include <chrono>
#include <stdexcept>
#include <assert.h>
#include <float.h>

#include "common.h"
#include "debug_timer.h"
#include "geom.h"
#include "image_stack.h"
#include "psf.h"
#include "psi_phi_array_ds.h"
#include "psi_phi_array_utils.h"
#include "pydocs/stack_search_docs.h"
#include "stamp_creator.h"

namespace search {
using Point = indexing::Point;
using Image = search::Image;

class StackSearch {
public:
    StackSearch(ImageStack& imstack);

    int num_images() const { return stack.img_count(); }
    int get_image_width() const { return stack.get_width(); }
    int get_image_height() const { return stack.get_height(); }
    int get_image_npixels() const { return stack.get_npixels(); }
    const ImageStack& get_imagestack() const { return stack; }

    // Parameter setters used to control the searches.
    void set_debug(bool d);
    void set_min_obs(int new_value);
    void set_min_lh(float new_value);
    void enable_gpu_sigmag_filter(std::vector<float> percentiles, float sigmag_coeff, float min_lh);
    void enable_gpu_encoding(int num_bytes);
    void set_start_bounds_x(int x_min, int x_max);
    void set_start_bounds_y(int y_min, int y_max);

    // The primary search functions
    void evaluate_single_trajectory(Trajectory& trj);
    Trajectory search_linear_trajectory(short x, short y, float vx, float vy);
    void search(int a_steps, int v_steps, float min_angle, float max_angle, float min_velocity,
                float max_velocity, int min_observations);

    // Gets the vector of result trajectories from the grid search.
    std::vector<Trajectory> get_results(int start, int end);

    // Getters for the Psi and Phi data.
    std::vector<float> get_psi_curves(Trajectory& t);
    std::vector<float> get_phi_curves(Trajectory& t);

    // Helper functions for computing Psi and Phi
    void prepare_psi_phi();
    void clear_psi_phi();

    // Helper functions for testing
    void set_results(const std::vector<Trajectory>& new_results);
    void clear_results();

    virtual ~StackSearch(){};

protected:
    void sort_results();

    // Creates list of trajectories to search.
    std::vector<Trajectory> create_grid_search_list(int angle_steps, int velocity_steps, float min_ang,
                                                    float max_ang, float min_vel, float mavx);

    std::vector<float> extract_psi_or_phi_curve(Trajectory& trj, bool extract_psi);

    // Core data and search parameters
    ImageStack stack;
    SearchParameters params;
    bool debug_info;

    // Precomputed and cached search data
    bool psi_phi_generated;
    PsiPhiArray psi_phi_array;

    // Cached data for grid search. TODO: see if we can remove this.
    std::vector<Trajectory> results;
};

} /* namespace search */

#endif /* KBMODSEARCH_H_ */
