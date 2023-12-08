import unittest

import numpy as np

from kbmod.search import (
    KB_NO_DATA,
    PsiPhi,
    PsiPhiArray,
    RawImage,
    compute_scale_params_from_image_vect,
    decode_uint_scalar,
    encode_uint_scalar,
    fill_psi_phi_array,
)


class test_psi_phi_array(unittest.TestCase):
    def setUp(self):
        self.num_times = 2
        self.width = 4
        self.height = 5

        psi_1_vals = np.arange(0, self.width * self.height, dtype=np.single)
        psi_1_arr = psi_1_vals.reshape(self.height, self.width)
        self.psi_1 = RawImage(img=psi_1_arr, obs_time=1.0)

        psi_2_vals = np.arange(self.width * self.height, 2 * self.width * self.height, dtype=np.single)
        psi_2_arr = psi_2_vals.reshape(self.height, self.width)
        self.psi_2 = RawImage(img=psi_2_arr, obs_time=2.0)

        self.phi_1 = RawImage(np.full((self.height, self.width), 0.1, dtype=np.single), obs_time=1.0)
        self.phi_2 = RawImage(np.full((self.height, self.width), 0.2, dtype=np.single), obs_time=2.0)

    def test_set_meta_data(self):
        arr = PsiPhiArray()
        self.assertEqual(arr.num_times, 0)
        self.assertEqual(arr.num_bytes, -1)
        self.assertEqual(arr.width, 0)
        self.assertEqual(arr.height, 0)
        self.assertEqual(arr.pixels_per_image, 0)
        self.assertEqual(arr.num_entries, 0)
        self.assertEqual(arr.block_size, 0)
        self.assertEqual(arr.total_array_size, 0)

        # Make float
        arr.set_meta_data(-1, self.num_times, self.width, self.height)
        self.assertEqual(arr.num_bytes, -1)
        self.assertEqual(arr.block_size, 4)
        self.assertEqual(arr.num_times, self.num_times)
        self.assertEqual(arr.width, self.width)
        self.assertEqual(arr.height, self.height)
        self.assertEqual(arr.pixels_per_image, self.width * self.height)
        self.assertEqual(arr.num_entries, 2 * self.width * self.height * self.num_times)
        self.assertEqual(arr.total_array_size, 4 * arr.num_entries)

        # Make uint8
        arr.set_meta_data(1, self.num_times, self.width, self.height)
        self.assertEqual(arr.num_bytes, 1)
        self.assertEqual(arr.block_size, 1)
        self.assertEqual(arr.num_times, self.num_times)
        self.assertEqual(arr.width, self.width)
        self.assertEqual(arr.height, self.height)
        self.assertEqual(arr.pixels_per_image, self.width * self.height)
        self.assertEqual(arr.num_entries, 2 * self.width * self.height * self.num_times)
        self.assertEqual(arr.total_array_size, 1 * arr.num_entries)

        # Make uint16
        arr.set_meta_data(2, self.num_times, self.width, self.height)
        self.assertEqual(arr.num_bytes, 2)
        self.assertEqual(arr.block_size, 2)
        self.assertEqual(arr.num_times, self.num_times)
        self.assertEqual(arr.width, self.width)
        self.assertEqual(arr.height, self.height)
        self.assertEqual(arr.pixels_per_image, self.width * self.height)
        self.assertEqual(arr.num_entries, 2 * self.width * self.height * self.num_times)
        self.assertEqual(arr.total_array_size, 2 * arr.num_entries)

    def test_decode_uint_scalar(self):
        self.assertAlmostEqual(decode_uint_scalar(1.0, 0.0, 5.0), 0.0)
        self.assertAlmostEqual(decode_uint_scalar(2.0, 0.0, 5.0), 5.0)
        self.assertAlmostEqual(decode_uint_scalar(3.0, 0.0, 5.0), 10.0)

        self.assertAlmostEqual(decode_uint_scalar(1.0, 2.5, 3.0), 2.5)
        self.assertAlmostEqual(decode_uint_scalar(2.0, 2.5, 3.0), 5.5)
        self.assertAlmostEqual(decode_uint_scalar(3.0, 2.5, 3.0), 8.5)

        # 0.0 always decodes to NO_DATA.
        self.assertAlmostEqual(decode_uint_scalar(0.0, 1.0, 5.0), KB_NO_DATA)

    def encode_uint_scalar(self):
        self.assertAlmostEqual(encode_uint_scalar(0.0, 0.0, 10.0, 0.1), 1.0)
        self.assertAlmostEqual(encode_uint_scalar(0.1, 0.0, 10.0, 0.1), 2.0)
        self.assertAlmostEqual(encode_uint_scalar(1.0, 0.0, 10.0, 0.1), 11.0)
        self.assertAlmostEqual(encode_uint_scalar(2.0, 0.0, 10.0, 0.1), 21.0)

        # NO_DATA always encodes to 0.0.
        self.assertAlmostEqual(encode_uint_scalar(KB_NO_DATA, 0.0, 10.0, 0.1), 0.0)

        # Test clipping.
        self.assertAlmostEqual(encode_uint_scalar(11.0, 0.0, 10.0, 0.1), 100.0)
        self.assertAlmostEqual(encode_uint_scalar(-100.0, 0.0, 10.0, 0.1), 1.0)

    def test_compute_scale_params_from_image_vect(self):
        max_val = 2 * self.width * self.height - 1

        # Parameters for encoding to a float
        result_float = compute_scale_params_from_image_vect([self.psi_1, self.psi_2], -1)
        self.assertAlmostEqual(result_float[0], 0.0, delta=1e-5)
        self.assertAlmostEqual(result_float[1], max_val, delta=1e-5)
        self.assertAlmostEqual(result_float[2], 1.0, delta=1e-5)

        # Parameters for encoding to an uint8
        result_uint8 = compute_scale_params_from_image_vect([self.psi_1, self.psi_2], 1)
        self.assertAlmostEqual(result_uint8[0], 0.0, delta=1e-5)
        self.assertAlmostEqual(result_uint8[1], max_val, delta=1e-5)
        self.assertAlmostEqual(result_uint8[2], max_val / 255.0, delta=1e-5)

        # Parameters for encoding to an uint16
        result_uint16 = compute_scale_params_from_image_vect([self.psi_1, self.psi_2], 2)
        self.assertAlmostEqual(result_uint16[0], 0.0, delta=1e-5)
        self.assertAlmostEqual(result_uint16[1], max_val, delta=1e-5)
        self.assertAlmostEqual(result_uint16[2], max_val / 65535.0, delta=1e-5)

    def test_fill_psi_phi_array(self):
        for num_bytes in [-1, 2]:
            arr = PsiPhiArray()
            fill_psi_phi_array(arr, num_bytes, [self.psi_1, self.psi_2], [self.phi_1, self.phi_2])

            # Check the meta data.
            self.assertEqual(arr.num_times, self.num_times)
            self.assertEqual(arr.num_bytes, num_bytes)
            self.assertEqual(arr.width, self.width)
            self.assertEqual(arr.height, self.height)
            self.assertEqual(arr.pixels_per_image, self.width * self.height)
            self.assertEqual(arr.num_entries, 2 * arr.pixels_per_image * self.num_times)
            if num_bytes == -1:
                self.assertEqual(arr.block_size, 4)
            else:
                self.assertEqual(arr.block_size, num_bytes)
            self.assertEqual(arr.total_array_size, arr.num_entries * arr.block_size)

            # Check that we can read the values from the CPU array.
            self.assertTrue(arr.cpu_array_allocated)
            self.assertTrue(arr.gpu_array_allocated)

            # Check that we can correctly read the values from the CPU.
            for time in range(self.num_times):
                offset = time * self.width * self.height
                for row in range(self.height):
                    for col in range(self.width):
                        val = arr.read_encoded_psi_phi(time, row, col, False)
                        self.assertAlmostEqual(val.psi, offset + row * self.width + col, delta=0.05)
                        self.assertAlmostEqual(val.phi, 0.1 * (time + 1), delta=1e-5)

            # Check that the arrays are set to NULL after we clear it (memory should be freed too).
            arr.clear()
            self.assertFalse(arr.cpu_array_allocated)
            self.assertFalse(arr.gpu_array_allocated)


if __name__ == "__main__":
    unittest.main()
