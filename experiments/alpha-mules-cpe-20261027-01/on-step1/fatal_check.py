"""Shared Python2/3 log check; a normal signal-trap enable banner is NOT a fault."""
from __future__ import print_function
import re,sys
def find_error(text):
 for line in text.replace('\r','').splitlines():
  s=line.lower()
  if any(word in s for word in ('foam fatal','mpi_abort','unknown cpe exception')):return line
  if re.search(r'residual\s*=\s*[-+]?(?:nan|inf)(?:[,\s]|$)',s):return line
  if any(word in s for word in ('segmentation fault','bus error','floating point exception')):
   if 'enabling' in s and 'trapping' in s:continue
   return line
 return None
if __name__=='__main__':
 try:
  with open(sys.argv[1]) as f:found=find_error(f.read())
 except IOError:sys.exit(2)
 if found:print(found);sys.exit(0)
 sys.exit(1)
