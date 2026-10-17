build:
	@./contrib/guix/guix-build

attest:
	@./contrib/guix/guix-attest

verify:
	@./contrib/guix/guix-verify

clean:
	@./contrib/guix/guix-clean

DEFAULT_GOAL := default
default: build

check:
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s contrib/docs/tests -v

.PHONY: default build attest verify clean check
