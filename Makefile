.PHONY: all fmt dev build test clean

all: build

fmt:
	cargo fmt
	cargo clippy

dev: fmt
	uv sync --group dev

build: dev
	uv run maturin develop

test: build
	uv run --frozen pytest

clean:
	rm -rf build dist target
	cargo clean
