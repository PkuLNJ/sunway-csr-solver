#!/bin/bash
set -euo pipefail
cd /home/export/online1/mdt00/shisuan/swpku_of/lnj/build/csr-qt-aligned-960-50-20261027-01
mkdir RUN_ONCE.guard
trap 'echo $? > RUN_EXIT_CODE' EXIT
bash prepare.sh
bash submit.sh
job=$(cat /home/export/online1/mdt00/shisuan/swpku_of/lnj/tests/csr-qt-aligned-960-50-20261027-01/JOB_ID)
bash monitor.sh "$job"
