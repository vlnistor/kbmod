import os
import tempfile
import unittest

from kbmod.file_utils import *
from kbmod.search import *


class test_file_utils(unittest.TestCase):
    def test_visit_from_file_name(self):
        visit = FileUtils.visit_from_file_name("m00005.fits")
        self.assertEqual(visit, "00005")

        visit = FileUtils.visit_from_file_name("m654321.fits")
        self.assertEqual(visit, "654321")

        # Too few digits
        visit = FileUtils.visit_from_file_name("m005.fits")
        self.assertIsNone(visit)

        # Nonsequential digits
        visit = FileUtils.visit_from_file_name("m123x45.fits")
        self.assertIsNone(visit)

    def test_save_load_csv(self):
        data = [[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]]
        with tempfile.TemporaryDirectory() as dir_name:
            file_name = f"{dir_name}/data1.dat"

            # Check that there is nothing to load before saving the file.
            # By default FileUtils should raise a FileNotFoundError.
            with self.assertRaises(FileNotFoundError):
                _ = FileUtils.load_csv_to_list(file_name)

            # Check that return None works when the file is missing.
            data0 = FileUtils.load_csv_to_list(file_name, none_if_missing=True)
            self.assertIsNone(data0)

            # Check the save function
            FileUtils.save_csv_from_list(file_name, data)
            self.assertTrue(Path(file_name).is_file())

            # Check the load function
            data2 = FileUtils.load_csv_to_list(file_name, use_dtype=float)
            self.assertEqual(len(data), len(data2))
            for i in range(len(data)):
                self.assertEqual(len(data[i]), len(data2[i]))
                for j in range(len(data[i])):
                    self.assertEqual(data[i][j], data2[i][j])

            # Check that we do not overwrite
            with self.assertRaises(ValueError):
                FileUtils.save_csv_from_list(file_name, data2)

    def test_load_times(self):
        times = FileUtils.load_time_dictionary("./data/fake_times.dat")
        self.assertEqual(len(times), 3)
        self.assertTrue("000003" in times)
        self.assertTrue("000005" in times)
        self.assertTrue("010006" in times)
        self.assertEqual(times["000003"], 57162.0)
        self.assertEqual(times["000005"], 57172.0)
        self.assertEqual(times["010006"], 100000.0)

    def test_save_times(self):
        mapping = {"0001": 100.0, "0002": 110.0, "0003": 111.0}
        with tempfile.TemporaryDirectory() as dir_name:
            file_name = f"{dir_name}/times.dat"
            FileUtils.save_time_dictionary(file_name, mapping)
            self.assertTrue(Path(file_name).is_file())

            loaded = FileUtils.load_time_dictionary(file_name)
            self.assertEqual(len(loaded), len(mapping))
            for k in mapping.keys():
                self.assertEqual(loaded[k], mapping[k])

    def test_load_psfs(self):
        psfs = FileUtils.load_psf_dictionary("./data/fake_psfs.dat")
        self.assertEqual(len(psfs), 2)
        self.assertTrue("000002" in psfs)
        self.assertTrue("000012" in psfs)
        self.assertEqual(psfs["000002"], 1.3)
        self.assertEqual(psfs["000012"], 1.5)

    def test_load_results(self):
        np_results = FileUtils.load_results_file("./data/fake_results.txt")
        self.assertEqual(len(np_results), 2)
        self.assertEqual(np_results[0]["x"], 106)
        self.assertEqual(np_results[0]["y"], 44)
        self.assertAlmostEqual(np_results[0]["vx"], 9.52)
        self.assertAlmostEqual(np_results[0]["vy"], -0.5)
        self.assertAlmostEqual(np_results[0]["lh"], 300.0)
        self.assertAlmostEqual(np_results[0]["flux"], 750.0)
        self.assertEqual(np_results[0]["num_obs"], 10)
        self.assertEqual(np_results[1]["x"], 55)
        self.assertEqual(np_results[1]["y"], 60)
        self.assertAlmostEqual(np_results[1]["vx"], 10.5)
        self.assertAlmostEqual(np_results[1]["vy"], -1.7)
        self.assertAlmostEqual(np_results[1]["lh"], 250.0)
        self.assertAlmostEqual(np_results[1]["flux"], 500.0)
        self.assertEqual(np_results[1]["num_obs"], 9)

    def test_load_results_trajectories(self):
        trj_results = FileUtils.load_results_file_as_trajectories("./data/fake_results.txt")
        self.assertEqual(len(trj_results), 2)

        self.assertTrue(isinstance(trj_results[0], trajectory))
        self.assertEqual(trj_results[0].x, 106)
        self.assertEqual(trj_results[0].y, 44)
        self.assertAlmostEqual(trj_results[0].x_v, 9.52, delta=1e-6)
        self.assertAlmostEqual(trj_results[0].y_v, -0.5, delta=1e-6)
        self.assertAlmostEqual(trj_results[0].lh, 300.0, delta=1e-6)
        self.assertAlmostEqual(trj_results[0].flux, 750.0, delta=1e-6)
        self.assertEqual(trj_results[0].obs_count, 10)

        self.assertTrue(isinstance(trj_results[1], trajectory))
        self.assertEqual(trj_results[1].x, 55)
        self.assertEqual(trj_results[1].y, 60)
        self.assertAlmostEqual(trj_results[1].x_v, 10.5, delta=1e-6)
        self.assertAlmostEqual(trj_results[1].y_v, -1.7, delta=1e-6)
        self.assertAlmostEqual(trj_results[1].lh, 250.0, delta=1e-6)
        self.assertAlmostEqual(trj_results[1].flux, 500.0, delta=1e-6)
        self.assertEqual(trj_results[1].obs_count, 9)

    def test_save_and_load_single_result(self):
        trj = trajectory()
        trj.x = 1
        trj.y = 2
        trj.x_v = 3.0
        trj.y_v = 4.0

        with tempfile.TemporaryDirectory() as dir_name:
            filename = f"{dir_name}/results_tmp.txt"
            FileUtils.save_results_file(filename, [trj])

            loaded_trjs = FileUtils.load_results_file_as_trajectories(filename)
            self.assertEqual(len(loaded_trjs), 1)
            self.assertEqual(loaded_trjs[0].x, trj.x)
            self.assertEqual(loaded_trjs[0].y, trj.y)
            self.assertEqual(loaded_trjs[0].x_v, trj.x_v)
            self.assertEqual(loaded_trjs[0].y_v, trj.y_v)


if __name__ == "__main__":
    unittest.main()