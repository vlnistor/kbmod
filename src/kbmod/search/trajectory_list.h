/*
 * trajectory_list.h
 *
 * The data structure for the raw results (a list of trajectories). The
 * data structure handles memory allocationon both the CPU and GPU and
 * transfering data between the two. It maintains ownership of the pointers
 * until clear() is called the object's destructor is called.
 *
 * Created on: March 7, 2024
 */

#ifndef TRAJECTORY_LIST_DS_
#define TRAJECTORY_LIST_DS_

#include <vector>

#include "common.h"

namespace search {

class TrajectoryList {
public:
    explicit TrajectoryList(int max_list_size);
    virtual ~TrajectoryList();

    // --- Getter functions ----------------
    inline int get_size() const { return max_size; }

    inline Trajectory& get_trajectory(int index) {
        if (index < 0 || index > max_size) throw std::runtime_error("Index out of bounds.");
        if (data_on_gpu) throw std::runtime_error("Data on GPU");
        return cpu_list[index];
    }

    inline void set_trajectory(int index, const Trajectory& new_value) {
        if (index < 0 || index > max_size) throw std::runtime_error("Index out of bounds.");
        if (data_on_gpu) throw std::runtime_error("Data on GPU");
        cpu_list[index] = new_value;
    }

    inline std::vector<Trajectory>& get_list() {
        if (data_on_gpu) throw std::runtime_error("Data on GPU");
        return cpu_list;
    }

    std::vector<Trajectory> get_batch(int start, int count);

    // Processing functions
    void sort_by_likelihood();
    void sort_by_obs_count();

    // Data allocation functions.
    inline bool on_gpu() const { return data_on_gpu; }
    void move_to_gpu();
    void move_to_cpu();

private:
    int max_size;
    bool data_on_gpu;

    std::vector<Trajectory> cpu_list;
    Trajectory* gpu_list_ptr = nullptr;
};

} /* namespace search */

#endif /* TRAJECTORY_LIST_DS_ */
