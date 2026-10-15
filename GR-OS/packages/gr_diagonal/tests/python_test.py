import ctypes
import unittest
from gr_diagonal import Diagonal, ExtensionError


class PackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.extension = Diagonal()

    def test_computation_lineage_and_lifetime(self):
        left, right = [2, -3, 0], [4, 5, 7]
        result = self.extension.apply(left, right)
        left[:] = []
        right[:] = []
        self.assertEqual(result.values, [8, -15, 0])
        self.assertEqual(result.report["package"]["id"], "org.gr.examples.diagonal")
        self.assertTrue(result.report["diagnostics"]["plan"])
        self.assertTrue(result.report["provenance"])
        self.assertEqual(self.extension.apply([], []).values, [])

    def test_domain_errors(self):
        with self.assertRaises(ExtensionError) as mismatch:
            self.extension.apply([2], [1, 2])
        self.assertEqual(mismatch.exception.code, "diagonal_contract")
        with self.assertRaises(ExtensionError) as overflow:
            self.extension.apply([2**63 - 1], [2])
        self.assertEqual(overflow.exception.code, "numeric_overflow")
        with self.assertRaises(TypeError):
            self.extension.apply([True], [1])
        with self.assertRaises(OverflowError):
            self.extension.apply([2**63], [1])
        with self.assertRaises(ValueError):
            self.extension.apply([], [], timeout_ms=True)

    def test_c_failure_preserves_output_and_deallocator(self):
        values = (ctypes.c_int64 * 1)(2**63 - 1)
        input = (ctypes.c_int64 * 1)(2)
        output = (ctypes.c_int64 * 1)(123)
        report, size = ctypes.c_void_p(), ctypes.c_size_t()
        status = self.extension.library.gr_diagonal_apply_i64_v1(
            values, 1, input, 1, output, 1, 0, ctypes.byref(report), ctypes.byref(size)
        )
        try:
            self.assertEqual(status, 1)
            self.assertEqual(output[0], 123)
            self.assertTrue(report.value)
        finally:
            self.extension.library.gr_diagonal_free(report)


if __name__ == "__main__":
    unittest.main()
