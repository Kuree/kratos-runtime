#!/usr/bin/env bash
set -xe

docker pull keyiz/manylinux
docker run -d --name manylinux --rm -it --mount type=bind,source="$(pwd)"/../kratos-runtime,target=/kratos-runtime keyiz/manylinux bash

docker exec -i manylinux bash -c 'cd kratos-runtime && python setup.py bdist_wheel'
docker exec -i manylinux bash -c 'cd kratos-runtime && auditwheel show dist/*'
docker exec -i manylinux bash -c 'cd kratos-runtime && auditwheel repair dist/*'
# remove the version dependencies
docker exec -i manylinux bash -c 'cd /kratos-runtime/scripts && pip install wheeltools && ./fix_wheel.sh'
# run in a new container where we have verilator installed
docker run -d --name test --rm -it --mount type=bind,source="$(pwd)"/../kratos-runtime,target=/kratos-runtime keyiz/garnet-flow bash
# install the python wheel and test it using the mock
ls -l scripts/dist/
docker exec -i test bash -c "cd /kratos-runtime/scripts && pip install dist/*.whl"
docker exec -i test bash -c "python -m pip install pytest twine"
docker exec -i test bash -c "cd /kratos-runtime && python -m pytest tests/"


echo [distutils]                                  > ~/.pypirc
echo index-servers =                             >> ~/.pypirc
echo "  pypi"                                    >> ~/.pypirc
echo                                             >> ~/.pypirc
echo [pypi]                                      >> ~/.pypirc
echo repository=https://upload.pypi.org/legacy/  >> ~/.pypirc
echo username=keyi                               >> ~/.pypirc
echo password=$PYPI_PASSWORD                     >> ~/.pypirc

