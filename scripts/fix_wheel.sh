#!/bin/bash

# use scikit-build script to add generic tag to the build
curl -L https://github.com/scikit-build/cmake-python-distributions/raw/master/scripts/convert_to_generic_platform_wheel.py > fix_wheel.py
mkdir -p dist
python fix_wheel.py ../wheelhouse/*.whl
