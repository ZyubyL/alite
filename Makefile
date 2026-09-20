all: build

dev:
	uv sync --group dev

build: dev setup.py src/alite/_core/*.c src/alite/_core/*.h
	uv run python setup.py build_ext --inplace

test: build
	uv run --frozen pytest

clean:
	rm -rf build
