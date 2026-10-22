.PHONY: all dev build test clean

all: build

dev:
	uv sync --group dev

build: dev
	uv run maturin develop

test: build
	uv run --frozen pytest

clean:
	rm -rf build dist target
	cargo clean
