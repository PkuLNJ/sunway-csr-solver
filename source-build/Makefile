.PHONY: ldu host cpe libraries-ldu libraries-csr
JOBS ?= 4
libraries-ldu:
	JOBS=$(JOBS) bash build-libraries.sh ldu
libraries-csr:
	JOBS=$(JOBS) bash build-libraries.sh csr
ldu: libraries-ldu
	bash ldu/build-solver.sh
host: libraries-csr
	$(MAKE) -C csr -j$(JOBS) solver BACKEND=host
cpe: libraries-csr
	$(MAKE) -C csr -j$(JOBS) solver BACKEND=cpe
