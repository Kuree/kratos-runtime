#!/usr/bin/env bash
set -xe

if [[ "$OS" == "linux" ]]; then
    docker pull keyiz/manylinux
    docker run -d --name manylinux --rm -it --mount type=bind,source="$(pwd)"/../kratos-runtime,target=/kratos-runtime keyiz/manylinux bash

    docker exec -i manylinux bash -c 'cd kratos-runtime && python setup.py bdist_wheel'
    docker exec -i manylinux bash -c 'cd kratos-runtime && auditwheel show dist/*'
    docker exec -i manylinux bash -c 'cd kratos-runtime && auditwheel repair dist/*'
    # remove the version dependencies
    docker exec -i manylinux bash -c 'cd /kratos-runtime/scripts && pip install wheeltools && ./fix_wheel.sh'
    # run the kratos container
    docker run -d --name test --rm -it --mount type=bind,source="$(pwd)"/../kratos-runtime,target=/kratos-runtime keyiz/kratos bash
    # install verilator
    docker exec -i test bash -c "apt update && apt install -y build-essential verilator"
    # install the python wheel and test it using the mock
    docker exec -i test bash -c "python3 -m pip install setuptools wheel"
    docker exec -i test bash -c "cd /kratos-runtime/scripts && python3 -m pip install dist/*.whl"
    docker exec -i test bash -c "python3 -m pip install pytest twine kratos && rm /usr/local/bin/pytest"
    docker exec -i test bash -c "cp -r /kratos-runtime/tests/ /tests/ && python3 -m pytest /tests/"

elif [[ "$OS" == "osx" ]]; then
    wget --quiet https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-x86_64.sh -O miniconda.sh
    chmod +x miniconda.sh
    ./miniconda.sh -b -p $HOME/miniconda
    export PATH=$HOME/miniconda/bin:$PATH
    conda config --set always_yes yes --set changeps1 no
    conda create -q -n env3.7 python=3.7
    source activate env3.7
    conda install pip
    python --version

    python -m pip install scikit-build
    python -m pip install cmake twine wheel pytest wheeltools
    CXX=/usr/local/bin/g++-8 python setup.py bdist_wheel
    # fix the wheel
    cd scripts && ./fix_wheel.sh && cd ..
    pip install scripts/dist/*.whl
    pytest -v tests/
fi

echo [distutils]                                  > ~/.pypirc
echo index-servers =                             >> ~/.pypirc
echo "  pypi"                                    >> ~/.pypirc
echo                                             >> ~/.pypirc
echo [pypi]                                      >> ~/.pypirc
echo repository=https://upload.pypi.org/legacy/  >> ~/.pypirc
echo username=keyi                               >> ~/.pypirc
echo password=$PYPI_PASSWORD                     >> ~/.pypirc

