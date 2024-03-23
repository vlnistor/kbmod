/*
 * kernel_memory.h
 *
 * Helper functions for transfering KBMOD data to/from GPU.
 */

#ifndef KERNELS_MEMORY_H_
#define KERNELS_MEMORY_H_

namespace search {

// ---------------------------------------
// --- Basic Memory Functions ------------
// ---------------------------------------

extern "C" void* allocate_gpu_block(unsigned long memory_size);

extern "C" void free_gpu_block(void* gpu_ptr);

extern "C" void copy_block_to_gpu(void* cpu_ptr, void* gpu_ptr, unsigned long memory_size);

extern "C" void copy_block_to_cpu(void* cpu_ptr, void* gpu_ptr, unsigned long memory_size);

} /* namespace search */

#endif /* KERNELS_MEMORY_CU_ */