import importlib.util,unittest
from pathlib import Path
p=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('timing',p/'summarize.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def log(n=50):
 return '\n'.join('Time = %.3f\nLoop Time = %s s\nExecutionTime = 50 s ClockTime = 60 s'%((i+1)*.001,i+1) for i in range(n))+'\nEnd\n'
class Timing(unittest.TestCase):
 def test_windows(self):
  r=m.parse(log());self.assertEqual(r['loop_time_sum50'],1275);self.assertEqual(r['windows'][1]['sum_seconds'],54);self.assertEqual(r['windows'][2]['count'],48)
 def test_crlf(self):self.assertEqual(m.parse(log().replace('\n','\r\n'))['steps'][-1]['step'],50)
 def test_short_fails(self):
  with self.assertRaises(AssertionError):m.parse(log(49))
 def test_missing_end_fails(self):
  with self.assertRaises(AssertionError):m.parse(log().replace('\nEnd\n',''))
 def test_bad_time_fails(self):
  with self.assertRaises(AssertionError):m.parse(log().replace('Time = 0.030','Time = 0.031'))
 def test_fatal_fails(self):
  with self.assertRaises(AssertionError):m.parse(log()+'\nFOAM FATAL ERROR\n')
 def test_normal_trapping_banner_passes(self):
  banner='sigFpe : Enabling floating point exception trapping (FOAM_SIGFPE).\n'
  self.assertEqual(m.parse(banner+log())['steps'][-1]['step'],50)
 def test_actual_fpe_fault_fails(self):
  with self.assertRaises(AssertionError):m.parse(log()+'\n[31] Floating point exception (core dumped)\n')
if __name__=='__main__':unittest.main()
