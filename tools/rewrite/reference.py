"""Executable specification of the new LCR measurement maths (standard library only).

Phasors follow x[n] = dc + Re(V * exp(j * omega * n)).
These routines are a host reference, not a claim of firmware or hardware readiness.
"""
from __future__ import annotations

from dataclasses import dataclass
import cmath
import math
from typing import Sequence


@dataclass(frozen=True)
class Frequency:
    nominal_hz: int
    timer_divisor: int
    count: int

    @property
    def word(self) -> int:
        return (self.nominal_hz * (1 << 28) + 4_000_000) // 8_000_000

    @property
    def actual_hz(self) -> float:
        return self.word * 8_000_000 / (1 << 28)

    @property
    def sample_hz(self) -> float:
        return 84_000_000 / self.timer_divisor

    @property
    def omega(self) -> float:
        # DDS and timer share HSE; the unknown absolute HSE error cancels here.
        return 2 * math.pi * self.word * self.timer_divisor / ((1 << 28) * 10.5)

    def dds_words(self) -> tuple[int, ...]:
        return (0x2100, 0x4000 | (self.word & 0x3FFF), 0x4000 | (self.word >> 14), 0xC000, 0x2000)


FREQUENCIES = (
    Frequency(100, 52500, 128),
    Frequency(1000, 5250, 256),
    Frequency(10000, 525, 512),
    Frequency(100000, 70, 1200),
)
RF = (10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0)
RANGE_MASKS = (0x09, 0x0A, 0x0C, 0x00, 0x18, 0x28)


@dataclass(frozen=True)
class Fit:
    phasor: complex
    dc: float
    residual_rms: float
    minimum: float
    maximum: float
    clipped: bool

    @property
    def rms(self) -> float:
        return abs(self.phasor) / math.sqrt(2)


def solve3(matrix: list[list[float]], rhs: list[float]) -> list[float]:
    a = [list(row) + [rhs[i]] for i, row in enumerate(matrix)]
    for col in range(3):
        pivot = max(range(col, 3), key=lambda i: abs(a[i][col]))
        if abs(a[pivot][col]) < 1e-10:
            raise ValueError("Ill-conditioned sine fit")
        a[col], a[pivot] = a[pivot], a[col]
        scale = a[col][col]
        a[col] = [v / scale for v in a[col]]
        for row in range(3):
            if row != col:
                scale = a[row][col]
                a[row] = [x - scale * y for x, y in zip(a[row], a[col])]
    return [a[i][3] for i in range(3)]


def fit(samples: Sequence[float], omega: float) -> Fit:
    if len(samples) < 8 or not math.isfinite(omega) or not 0 < omega < math.pi:
        raise ValueError("Invalid frame geometry")
    if not all(math.isfinite(x) and 0 <= x <= 4095 for x in samples):
        raise ValueError("Invalid ADC sample")
    basis = [(1.0, math.cos(omega * i), math.sin(omega * i)) for i in range(len(samples))]
    gram = [[math.fsum(v[r] * v[c] for v in basis) for c in range(3)] for r in range(3)]
    rhs = [math.fsum(v[c] * x for v, x in zip(basis, samples)) for c in range(3)]
    dc, cosine, sine = solve3(gram, rhs)
    residual = math.sqrt(math.fsum((x - dc - cosine * v[1] - sine * v[2]) ** 2 for v, x in zip(basis, samples)) / len(samples))
    return Fit(complex(cosine, -sine), dc, residual, min(samples), max(samples), min(samples) <= 8 or max(samples) >= 4087)


def feedback(rf: float, cf: float, hz: float) -> complex:
    if not all(math.isfinite(x) for x in (rf, cf, hz)) or rf <= 0 or cf < 0 or hz <= 0:
        raise ValueError("Invalid feedback model")
    return 1 / complex(1 / rf, 2 * math.pi * hz * cf)


def impedance(v1: Fit, v2: Fit, zf: complex, alpha: complex = 1 + 0j) -> complex:
    if v1.clipped or v2.clipped:
        raise ValueError("Clipped frame")
    if not all(math.isfinite(v.real) and math.isfinite(v.imag) for v in (zf, alpha)) or abs(alpha) < 1e-12 or abs(zf) < 1e-12:
        raise ValueError("Invalid calibration")
    # Noise gates are explicit and provisional; board data must set final limits.
    for value in (v1, v2):
        if value.rms < max(2.0, 10 * value.residual_rms):
            raise ValueError("Signal too weak or distorted")
    return -zf * v1.phasor / v2.phasor / alpha


def compensate(zm: complex, zs: complex, yo: complex) -> complex:
    # Model: measured impedance = Zs + 1 / (Ydut + Yo).
    # Store open admittance, avoiding an enormous or infinite open impedance.
    values = (zm, zs, yo)
    if not all(math.isfinite(v.real) and math.isfinite(v.imag) for v in values):
        raise ValueError("Non-finite compensation input")
    delta = zm - zs
    denominator = 1 - yo * delta
    if abs(denominator) < 1e-9:
        raise ValueError("Open or ill-conditioned compensation")
    return delta / denominator


def open_admittance(v1: Fit, v2: Fit, zf: complex, alpha: complex = 1 + 0j) -> complex:
    """Raw measured admittance for open acquisition; low V2 is expected here.

    This is not yet fixture Yo: if series residual Zs is known, use
    Yo = Yopen / (1 - Zs * Yopen). Average complex frames before that transform.
    """
    if v1.clipped or v2.clipped or v1.rms < max(2.0, 10 * v1.residual_rms):
        raise ValueError("Invalid open-calibration frame")
    if not all(math.isfinite(v.real) and math.isfinite(v.imag) for v in (zf, alpha)) or abs(zf) < 1e-12 or abs(alpha) < 1e-12:
        raise ValueError("Invalid calibration")
    return -alpha * v2.phasor / (zf * v1.phasor)


def series_parameters(z: complex, hz: float) -> dict:
    if hz <= 0 or not math.isfinite(hz) or not math.isfinite(z.real) or not math.isfinite(z.imag):
        raise ValueError("Invalid impedance")
    result = {"rs_ohm": z.real, "xs_ohm": z.imag}
    if z.imag > 0:
        result["ls_h"] = z.imag / (2 * math.pi * hz)
    elif z.imag < 0:
        result["cs_f"] = -1 / (2 * math.pi * hz * z.imag)
    # This conversion deliberately does not declare an automatic component type.
    return result


def relay_transition(current: int, target: int) -> tuple[int, int, int]:
    if current not in range(6) or target not in range(6):
        raise ValueError("Invalid range")
    # Abstract coil masks, each separated by a verified mechanical settle time.
    # Caller MUST mute before executing; 0x08 is a temporary open feedback arm.
    return RANGE_MASKS[current], 0x08, RANGE_MASKS[target]
