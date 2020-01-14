#!/usr/bin/env bash
set -e

docker cp ~/.pypirc manylinux:/home/
docker exec -i manylinux bash -c 'cd /kratos-runtime/scripts && twine upload --config-file /home/.pypirc dist/*'
