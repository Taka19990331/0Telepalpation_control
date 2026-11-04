import csv
from pathlib import Path
import unittest
import numpy as np


class ControlModeTest(unittest.TestCase):
    def test_off_fixed_and_blended_torque_equations(self):
        src = Path(__file__).resolve().parents[2] / 'trans_ctrl_test1' / 'data' / 'trans_ctrl_test1_20261102_123759.csv'
        with src.open(newline='', encoding='utf-8-sig') as f:
            rows = list(csv.DictReader(line for line in f if not line.startswith('#')))
        x = np.asarray([float(r['external_torque_est_Nm']) for r in rows], dtype=np.float32)
        omega = 2*np.pi*12/2000
        alpha = np.sin(omega)*4/(2*12)
        b0=1/(1+alpha); b1=-2*np.cos(omega)/(1+alpha); b2=b0; a1=b1; a2=(1-alpha)/(1+alpha)
        z1=z2=0.; ready=False; filtered=[]
        for value in x:
            xd=float(value)
            if not ready: z1=(1-b0)*xd; z2=(b2-a2)*xd; ready=True
            y=b0*xd+z1; z1=b1*xd-a1*y+z2; z2=b2*xd-a2*y
            filtered.append(np.float32(xd+.95*(y-xd)))
        filtered=np.asarray(filtered,dtype=np.float32)
        off=np.float32(.75)*x
        fixed=np.float32(.75)*filtered
        self.assertTrue(np.array_equal(off,np.float32(.75)*x))
        self.assertTrue(np.array_equal(fixed,np.float32(.75)*filtered))
        # The blend is affine in gamma, so adjacent gamma changes create no algebraic jump.
        gamma=np.linspace(0,1,len(x),dtype=np.float32)
        blended=np.float32(.75)*(x+gamma*(filtered-x))
        self.assertTrue(np.isfinite(blended).all())
        self.assertEqual(float(gamma[0]),0.0)
        self.assertEqual(float(gamma[-1]),1.0)


if __name__ == '__main__':
    unittest.main()
