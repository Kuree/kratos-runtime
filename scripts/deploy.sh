#!/usr/bin/env bash
set -e

docker cp ~/.pypirc manylinux:/home/
docker exec -i manylinux bash -c 'cd /kratos-runtime && for PYBIN in cp35 cp36 cp37; do /opt/python/${PYBIN}-${PYBIN}m/bin/python setup.py bdist_wheel; done'
docker exec -i manylinux bash -c 'cd /kratos-runtime && for WHEEL in dist/*.whl; do auditwheel repair "${WHEEL}"; done'
docker exec -i manylinux bash -c 'cd /kratos-runtime && twine upload --config-file /home/.pypirc wheelhouse/*'
