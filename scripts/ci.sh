#!/usr/bin/env bash
set -xe

docker pull keyiz/manylinux
docker run -d --name manylinux --rm -it --mount type=bind,source="$(pwd)"/../kratos-runtime,target=/kratos-runtime keyiz/manylinux bash

docker exec -i manylinux bash -c 'cd kratos-runtime && python setup.py bdist_wheel'
docker exec -i manylinux bash -c 'cd kratos-runtime && auditwheel show dist/*'
docker exec -i manylinux bash -c 'cd kratos-runtime && auditwheel repair dist/*'


echo [distutils]                                  > ~/.pypirc
echo index-servers =                             >> ~/.pypirc
echo "  pypi"                                    >> ~/.pypirc
echo                                             >> ~/.pypirc
echo [pypi]                                      >> ~/.pypirc
echo repository=https://upload.pypi.org/legacy/  >> ~/.pypirc
echo username=keyi                               >> ~/.pypirc
echo password=$PYPI_PASSWORD                     >> ~/.pypirc

