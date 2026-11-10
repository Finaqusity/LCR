import sys, os
sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_docx as B

tests = [
    r"\dfrac{-1}{\omega X}=\dfrac{1}{\omega|X|}",
    r"\dfrac{Z_f}{Z_x}",
    r"|Z|=\dfrac{1}{2\pi f C}",
    r"Z_x=-\dfrac{Z_f}{A}\cdot\dfrac{1}{1+Z_f/Z_x}",
    r"\sqrt{R^2+X^2}",
    r"Q=\dfrac{\omega L}{R}",
    r"\varepsilon=\dfrac{1+Z_f/Z_x}{A}",
    r"R_s=\dfrac{1}{\omega C_s D}",
    r"\Delta\theta=360^\circ f\Delta t",
    r"\dfrac{V_o-V_{REF}}{V_{in}-V_{REF}}",
    r"2\pi\times10^5\times10\times10^{-12}",
    r"10^5/2\pi\approx159\,\text{k}\Omega",
]
for t in tests:
    print("IN :", t)
    print("OUT:", B.math_plain(t))
    print()
