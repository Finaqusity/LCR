"""Independent synthetic circuits and connection checks for the rewrite."""
import cmath
import json
import math
from pathlib import Path
import random
import unittest

from reference import FREQUENCIES, RF, RANGE_MASKS, compensate, feedback, fit, impedance, open_admittance, relay_transition, series_parameters


def signal(phasor, dc, omega, n, noise=0.0, seed=0, harmonic=0.0, quantize=True):
    rng = random.Random(seed)
    values = [dc + (phasor * cmath.exp(1j * omega * i)).real + harmonic * math.cos(3 * omega * i) + rng.gauss(0, noise) for i in range(n)]
    return [max(0, min(4095, round(v) if quantize else v)) for v in values]


class MeasurementTests(unittest.TestCase):
    def test_dds_frequency_error_and_register_address(self):
        for f in FREQUENCIES:
            with self.subTest(hz=f.nominal_hz):
                self.assertLess(abs(f.actual_hz / f.nominal_hz - 1), 0.001)
                words = f.dds_words()
                self.assertEqual(words[1] >> 14, 1)
                self.assertEqual(words[2] >> 14, 1)
                self.assertEqual((words[1] & 0x3FFF) | ((words[2] & 0x3FFF) << 14), f.word)
                self.assertAlmostEqual(f.omega, 2 * math.pi * f.actual_hz / f.sample_hz)
                self.assertLess(f.sample_hz * 15, 21_000_000)

    def test_unknown_phase_and_independent_dc(self):
        for f in FREQUENCIES:
            for phase in (-2.5, -0.3, 1.7):
                v = 700 * cmath.exp(1j * phase)
                exact = fit(signal(v, 1900, f.omega, f.count, quantize=False), f.omega)
                self.assertLess(abs(exact.phasor / v - 1), 1e-12)
                result = fit(signal(v, 1900, f.omega, f.count), f.omega)
                # Repeated phase points need not average away ADC quantization.
                self.assertLess(abs(result.phasor - v), 1.0)
                self.assertLess(abs(result.dc - 1900), 0.5)

    def test_noninteger_cycle_frame(self):
        omega = 2 * math.pi / 15.7
        result = fit(signal(500 + 200j, 1700, omega, 137), omega)
        self.assertLess(abs(result.phasor - (500 + 200j)), 0.15)

    def test_circuit_endpoints_all_frequencies(self):
        for f in FREQUENCIES:
            w = 2 * math.pi * f.actual_hz
            circuits = [complex(r, 0) for r in (10, 100, 1000, 10000, 100000, 1000000)]
            circuits += [complex(0.15, -1 / (w * c)) for c in (10e-12, 100e-12, 1e-9, 100e-9, 10e-6)]
            circuits += [complex(0.3, w * l) for l in (10e-6, 100e-6, 1e-3, 100e-3)]
            for z in circuits:
                rf = min(RF, key=lambda r: abs(math.log10(r / abs(z))))
                v1 = 700 * cmath.exp(0.72j)
                v2 = -rf * v1 / z
                if abs(v2) > 1500 or abs(v2) < 30:
                    continue  # Outside this synthetic ADC frame's usable dynamic range.
                a = fit(signal(v1, 2100, f.omega, f.count, 0.5, 1), f.omega)
                b = fit(signal(v2, 1900, f.omega, f.count, 0.5, 2), f.omega)
                measured = impedance(a, b, complex(rf))
                with self.subTest(hz=f.nominal_hz, z=z):
                    self.assertLess(abs(measured - z) / abs(z), 0.005)

    def test_channel_calibration_divides_alpha(self):
        f = FREQUENCIES[3]
        g1, g2 = 1.04 * cmath.exp(0.04j), 0.97 * cmath.exp(-0.03j)
        common = 600 * cmath.exp(-0.3j)
        alpha = fit(signal(g1 * common, 2050, f.omega, f.count, quantize=False), f.omega).phasor / fit(signal(g2 * common, 2000, f.omega, f.count, quantize=False), f.omega).phasor
        z, zf = 500 - 600j, 1000 + 0j
        v = 600 * cmath.exp(0.9j)
        a = fit(signal(g1 * v, 2100, f.omega, f.count, quantize=False), f.omega)
        b = fit(signal(g2 * (-zf * v / z), 1900, f.omega, f.count, quantize=False), f.omega)
        self.assertLess(abs(impedance(a, b, zf, alpha) / z - 1), 1e-12)
        wrong = -zf * a.phasor / b.phasor * alpha
        self.assertGreater(abs(wrong / z - 1), 0.1)

    def test_parallel_feedback_capacitance(self):
        hz, r, c = 100000, 1000000, 2.2e-12
        expected = r / (1 + 1j * 2 * math.pi * hz * r * c)
        self.assertLess(abs(feedback(r, c, hz) / expected - 1), 1e-14)

    def test_open_short_network_recovery(self):
        zs, yo = 0.12 + 0.02j, 1e-9 + 1j * 2 * math.pi * 100000 * 1.2e-12
        for z in (10 + 0j, 1e6 + 0j, complex(0, -1 / (2 * math.pi * 100000 * 10e-12)), 3 + 70j):
            measured = zs + 1 / (1 / z + yo)
            self.assertLess(abs(compensate(measured, zs, yo) / z - 1), 1e-12)

    def test_open_compensation_rejects_singular_case(self):
        with self.assertRaises(ValueError):
            compensate(1e6 + 0j, 0j, 1e-6 + 0j)

    def test_open_acquisition_accepts_zero_current(self):
        f = FREQUENCIES[3]
        a = fit(signal(700, 2000, f.omega, f.count), f.omega)
        b = fit(signal(0, 2000, f.omega, f.count), f.omega)
        self.assertLess(abs(open_admittance(a, b, 100000 + 0j)), 1e-15)
        with self.assertRaises(ValueError):
            impedance(a, b, 100000 + 0j)

    def test_saturation_rejected(self):
        f = FREQUENCIES[0]
        good = fit(signal(700, 2000, f.omega, f.count), f.omega)
        bad = fit(signal(2500, 2000, f.omega, f.count), f.omega)
        with self.assertRaises(ValueError):
            impedance(good, bad, 1000 + 0j)

    def test_weak_signal_and_harmonic_distortion_rejected(self):
        f = FREQUENCIES[2]
        good = fit(signal(700, 2000, f.omega, f.count), f.omega)
        for samples in (signal(1, 2000, f.omega, f.count), signal(100, 2000, f.omega, f.count, harmonic=100)):
            with self.assertRaises(ValueError):
                impedance(good, fit(samples, f.omega), 1000 + 0j)

    def test_invalid_geometry_and_data(self):
        for samples, omega in (([0] * 4, 0.4), ([2000] * 16, 0), ([2000] * 16, math.nan), ([math.nan] * 16, 0.4), ([4096] * 16, 0.4)):
            with self.assertRaises(ValueError):
                fit(samples, omega)

    def test_invalid_calibration(self):
        f = FREQUENCIES[1]
        value = fit(signal(500, 2000, f.omega, f.count), f.omega)
        for zf, alpha in ((0j, 1+0j), (1000+0j, 0j), (1000+0j, complex(math.nan, 0))):
            with self.assertRaises(ValueError):
                impedance(value, value, zf, alpha)

    def test_component_units(self):
        f = 100000
        for l in (10e-6, 100e-3):
            self.assertAlmostEqual(series_parameters(0.5 + 1j * 2 * math.pi * f * l, f)["ls_h"] / l, 1)
        for c in (10e-12, 10e-6):
            self.assertAlmostEqual(series_parameters(0.5 - 1j / (2 * math.pi * f * c), f)["cs_f"] / c, 1)

    def test_relay_paths_do_not_overlap_feedback_resistors(self):
        def connected(mask):
            return [i for i in range(6) if (not mask & 8 if i == 3 else bool(mask & (1 << i)))]
        for i, mask in enumerate(RANGE_MASKS):
            self.assertEqual(connected(mask), [i])
            self.assertLessEqual(mask.bit_count(), 2)
        for start in range(6):
            for end in range(6):
                path = relay_transition(start, end)
                self.assertEqual(connected(path[0]), [start])
                self.assertEqual(connected(path[1]), [])
                self.assertEqual(connected(path[2]), [end])
        # Mechanical timing and contact bounce remain hardware validation items.


class AuthoritativeNetlistTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads((Path(__file__).resolve().parents[2] / "docs/rewrite/netlist_facts.json").read_text(encoding="utf-8"))

    def same_net(self, *pins):
        self.assertEqual(len({self.data["pins"][pin] for pin in pins}), 1)

    def test_buttons_and_external_pullups(self):
        self.same_net("PB1.1", "U11.36", "R11.2")
        self.same_net("PB2.1", "U11.35", "R13.2")
        self.same_net("R11.1", "R13.1", "U11.6")
        self.same_net("PB1.2", "PB2.2", "U11.10")

    def test_relay_driver_and_actual_references(self):
        # (MCU pin, ULN input, ULN output, relay coil, common, selected contact, resistor)
        routes = [(81,1,18,"K2",3,4,"R30"),(82,2,17,"K4",3,4,"R32"),(83,3,16,"K3",3,4,"R31"),(84,4,15,"K7",3,2,"R34"),(85,5,14,"K6",3,4,"R36"),(86,6,13,"K5",3,4,"R35")]
        for mcu, inp, out, relay, common, contact, resistor in routes:
            self.same_net(f"U11.{mcu}", f"U16.{inp}")
            self.same_net(f"U16.{out}", f"{relay}.8")
            self.same_net(f"{relay}.{common}", f"{resistor}.2")
            self.same_net(f"{relay}.{contact}", "U14.6")
            self.same_net(f"{resistor}.1", "U17.2")

    def test_channel_calibration_and_adc_routes(self):
        self.same_net("U11.87", "U16.7")
        self.same_net("U16.12", "K1.8")
        self.same_net("K1.4", "K1.5", "R18.2")
        self.same_net("U10.5", "F1.1")
        self.same_net("U10.6", "U10.7", "R18.1")
        self.same_net("K1.6", "R26.1")
        self.same_net("K1.3", "R27.2")
        self.same_net("R22.2", "U11.23")
        self.same_net("R24.2", "U11.24")

    def test_nc_capacitors_and_reference_decoupling(self):
        for ref in ("C46", "C47"):
            self.assertEqual(self.data["components"][ref]["value"], "NC")
        self.same_net("C54.2", "C55.2", "U17.3")
        self.same_net("C54.1", "C55.1", "U11.10")
        self.assertEqual(self.data["components"]["C54"]["value"], "100nF")
        self.assertEqual(self.data["components"]["C55"]["value"], "1uF")


if __name__ == "__main__":
    unittest.main(verbosity=2)
