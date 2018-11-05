import re
import os
import subprocess
import unittest
import tempfile
import json
import shutil

tempfile.tempdir = "/tmp"

def docker(args, assert_on_failure=True):
    result = subprocess.run(["sudo", "docker"] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if assert_on_failure and result.returncode != 0:
        print('STDOUT: ')
        print(result.stdout.decode())
        print('STDERR: ')
        print(result.stderr.decode())
        raise RuntimeError("`docker {}` exited with code {}".format(' '.join(args), result.returncode))

    return (result.stdout.decode(), result.stderr.decode(), result.returncode)

def get_server_config(**kwargs):
    return {
        "role": "server",
        "secret": "lovely",
        "address_pool": "10.179.0.0/24",
        "padding_to": 1000,
        "data_pipe_rotate_interval": 60
    }

def get_client_config(**kwargs):
    return {
        "role": "client",
        "server": "server",
        "secret": "lovely",
        "forward_subnets": [
        ],
        "excluded_subnets": [
        ]
    }

DOCKER_NETWORK_NAME = 'stun_test'
DOCKER_IMAGE_TAG = 'stun_test'

class Host():
    def __init__(self, name, config):
        self.name = name
        self.config = config

    def __enter__(self):
        td = tempfile.mkdtemp(prefix="stun_tes")

        with open(os.path.join(td, 'stunrc'), 'w') as f:
            json.dump(self.config, f)

        self.id = docker([
            "create",
            "--volume={}:/usr/config".format(td),
            "--cap-add=NET_ADMIN",
            "--net={}".format(DOCKER_NETWORK_NAME),
            "--net-alias={}".format(self.name),
            "--device=/dev/net/tun",
            DOCKER_IMAGE_TAG,
        ])[0].strip()

        docker(["start", self.id])

        return self

    def __exit__(self, *args):
        docker(["kill", self.id])
        docker(["rm", self.id])

    def exec(self, cmd):
        return docker(["exec", self.id] + cmd, assert_on_failure=False)

class TestBasic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory(prefix='stun_test_build') as td:
            shutil.copyfile(os.environ['DOCKERFILE_PATH'], os.path.join(td, 'Dockerfile'))
            shutil.copyfile(os.environ['BINARY_PATH'], os.path.join(td, 'stun'))
            docker(['build', '-t', DOCKER_IMAGE_TAG, td])

    def setUp(self):
        docker(['network', 'create', DOCKER_NETWORK_NAME])

    def tearDown(self):
        docker(['network', 'rm', DOCKER_NETWORK_NAME])

    def test_basic(self):
        with Host("server", get_server_config()) as server, \
            Host("client", get_client_config()) as client:

            self.assertEqual(
                client.exec(["ping", "-t", "1", "-c", "1", "10.179.0.1"])[2],
                0,
                "failed to ping from client to server."
            )
