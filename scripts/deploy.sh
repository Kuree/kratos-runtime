#!/usr/bin/env bash
set -e

if [[ "$OS" == "linux" ]]; then
    docker cp ~/.pypirc manylinux:/home/
    docker exec -i manylinux bash -c 'cd /kratos-runtime/scripts && twine upload --config-file /home/.pypirc dist/*'
else
    twine upload --skip-existing --config-file /home/.pypirc scripts/dist/*
fi
