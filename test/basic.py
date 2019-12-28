import re
import os
import subprocess
import unittest
import tempfile
import json
import shutil
import time
import uuid
import random

tempfile.tempdir = "/tmp"


def docker(args, assert_on_failure=True, input=None):
    result = subprocess.run(
        ["docker"] + args, input=input, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )

    if assert_on_failure and result.returncode != 0:
        print("STDOUT: ")
        print(result.stdout.decode())
        print("STDERR: ")
        print(result.stderr.decode())
        raise RuntimeError(
            "`docker {}` exited with code {}".format(" ".join(args), result.returncode)
        )

    return (result.stdout.decode(), result.stderr.decode(), result.returncode)


def get_server_config(test_context, **kwargs):
    return {
        "role": "server",
        "secret": "lovely",
        "address_pool": f"{test_context.get_ip(0)}/24",
        "padding_to": 1000,
        "data_pipe_rotate_interval": 60,
        **kwargs,
    }


def get_client_config(test_context, **kwargs):
    return {
        "role": "client",
        "server": "server",
        "secret": "lovely",
        "forward_subnets": [],
        "excluded_subnets": [],
        **kwargs,
    }


DOCKER_CONTAINER_NAME_PREFIX = "stun_test-"
DOCKER_NETWORK_NAME = "stun_test"
DOCKER_IMAGE_TAG = "stun_test"


class Context(object):
    def __init__(self):
        self.uuid = uuid.uuid1().hex
        self.subnet_id = random.randint(1, 254)

    def get_ip(self, last_octet):
        return f"10.179.{self.subnet_id}.{last_octet}"


class Host:
    def __init__(self, test_context, name, config, entry_args=[]):
        self.test_context = test_context
        self.name = name
        self.config = config
        self.entry_args = entry_args

    def __enter__(self):
        td = tempfile.mkdtemp(prefix="stun_tes")

        with open(os.path.join(td, "stunrc"), "w") as f:
            json.dump(self.config, f)

        args = ["-c", "/usr/config/stunrc"] + self.entry_args

        self.id = docker(
            [
                "create",
                "--name={}".format(
                    DOCKER_CONTAINER_NAME_PREFIX
                    + self.test_context.uuid
                    + "-"
                    + self.name
                ),
                "--volume={}:/usr/config".format(td),
                "--cap-add=NET_ADMIN",
                "--net={}".format(DOCKER_NETWORK_NAME + "-" + self.test_context.uuid),
                "--net-alias={}".format(self.name),
                "--device=/dev/net/tun",
                "--entrypoint=/usr/src/stun",
                DOCKER_IMAGE_TAG,
            ]
            + args
        )[0].strip()

        docker(["start", self.id])

        return self

    def __exit__(self, *args):
        docker(["kill", self.id], assert_on_failure=False)
        docker(["rm", self.id])

    def logs(self):
        return docker(["logs", self.id])[0]

    def get_messenger_payloads(self, type):
        regex = re.compile("Sent: {} = (.*)$".format(type))
        results = []
        for line in self.logs().split("\n"):
            match = regex.search(line)
            if match is not None:
                results.append(json.loads(match.group(1)))
        return results

    def exec(self, cmd, assert_on_failure=False, input=None, detach=False):
        extra_flags = ["--interactive"]

        if detach:
            extra_flags.append("-d")

        return docker(
            ["exec"] + extra_flags + [self.id] + cmd,
            assert_on_failure=assert_on_failure,
            input=input,
        )

    def create_file(self, path, content):
        self.exec(["tee", path], assert_on_failure=True, input=content)

    def get_client_tunnel_ip(self):
        lines = self.exec(["ifconfig"], assert_on_failure=True)[0].split("\n")

        for i in range(len(lines) - 1):
            if lines[i].startswith("tun0: "):
                match = re.search(
                    r"inet ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)", lines[i + 1]
                )
                if match is None:
                    raise RuntimeError("Unable to find client tunnel IP address.")
                return match[1]

        return None

    def run_stun(self, name, config):
        """Run an additional `stun` process with the given name and config. 
        Return the PID of the launched `stun` process."""
        config_path = "/usr/config/stunrc-{}".format(name)
        self.create_file(config_path, json.dumps(config).encode())

        self.exec(
            ["/usr/src/stun", "-c", config_path], assert_on_failure=True, detach=True
        )

        time.sleep(1)

        return self.get_stun_pid(name)

    def get_stun_pid(self, name):
        """Return the PID of the `stun` process previously launched with
        `run_stun` with the given name."""
        config_path = "/usr/config/stunrc-{}".format(name)
        ps_entries = self.exec(["ps", "aux"], assert_on_failure=True)[0].split("\n")
        ps_entries = [e for e in ps_entries if "-c {}".format(config_path) in e]

        if len(ps_entries) != 1:
            raise RuntimeError("Failed to get stun PID.")

        return int(ps_entries[0].split()[1])

    def kill_stun(self, name):
        """Kill a `stun` process previously launched with `run_stun` with the
        given name."""
        self.exec(["kill", str(self.get_stun_pid(name))], assert_on_failure=True)
        time.sleep(1)

    def get_masqueraded_subnets(self):
        """Return a list of subnets corresponding to iptables MASQUERADE rules
        created by `stun`."""
        entries = self.exec(
            ["iptables", "-t", "nat", "-L", "POSTROUTING"], assert_on_failure=True
        )[0].split("\n")

        entries = [e for e in entries if e.startswith("MASQUERADE")]
        entries = [e for e in entries if "/* stun" in e]

        return [e.split()[3] for e in entries]


class TestBasic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory(prefix="stun_test_build") as td:
            shutil.copyfile(
                os.environ["DOCKERFILE_PATH"], os.path.join(td, "Dockerfile")
            )
            shutil.copyfile(os.environ["BINARY_PATH"], os.path.join(td, "stun"))
            docker(["build", "-t", DOCKER_IMAGE_TAG, td])

    def setUp(self):
        self.test_context = Context()
        docker(
            ["network", "create", DOCKER_NETWORK_NAME + "-" + self.test_context.uuid]
        )

    def tearDown(self):
        docker(["network", "rm", DOCKER_NETWORK_NAME + "-" + self.test_context.uuid])

    def test_basic(self):
        with Host(
            self.test_context, "server", get_server_config(self.test_context)
        ) as server, Host(
            self.test_context, "client", get_client_config(self.test_context)
        ) as client:

            self.assertEqual(
                client.exec(
                    ["ping", "-t", "1", "-c", "1", f"{self.test_context.get_ip(1)}"]
                )[2],
                0,
                "Failed to ping from client to server.",
            )

    def test_static_hosts(self):
        server_config = get_server_config(
            self.test_context,
            authentication=True,
            static_hosts={"nice_guy": f"{self.test_context.get_ip(152)}"},
        )

        client_config = get_client_config(self.test_context, user="nice_guy")

        with Host(self.test_context, "server", server_config) as server, Host(
            self.test_context, "client", client_config
        ) as client:

            self.assertEqual(
                client.exec(
                    ["ping", "-t", "1", "-c", "1", f"{self.test_context.get_ip(1)}"]
                )[2],
                0,
                "Failed to ping from client to server.",
            )

            self.assertEqual(
                client.get_client_tunnel_ip(),
                f"{self.test_context.get_ip(152)}",
                "Unexpected IP assigned to client.",
            )

    def test_authentication_without_username(self):
        with Host(
            self.test_context,
            "server",
            get_server_config(self.test_context, authentication=True),
        ) as server, Host(
            self.test_context, "client", get_client_config(self.test_context)
        ) as client:

            self.assertIn(
                "Session ended with error: No user name provided.",
                client.logs(),
                "Client should have received the disconnect reason.",
            )

            self.assertEqual(
                client.exec(
                    ["ping", "-t", "1", "-c", "1", f"{self.test_context.get_ip(1)}"]
                )[2],
                1,
                "Should not be able to ping from client to server.",
            )

    def test_provided_subnets(self):
        with Host(
            self.test_context, "server", get_server_config(self.test_context)
        ) as server, Host(
            self.test_context,
            "client",
            get_client_config(self.test_context, provided_subnets=["10.180.0.0/24"]),
        ) as client:

            self.assertEqual(
                client.exec(
                    ["ping", "-t", "1", "-c", "1", f"{self.test_context.get_ip(1)}"]
                )[2],
                0,
                "Failed to ping from client to server.",
            )

            route_lines = server.exec(["ip", "route"], assert_on_failure=True)[0].split(
                "\n"
            )
            self.assertIn(
                f"10.180.0.0/24 via {self.test_context.get_ip(2)} dev tun0 ",
                route_lines,
                "Server should have added a route for the client-provided subnet.",
            )

    def test_custom_port(self):
        with Host(
            self.test_context, "server", get_server_config(self.test_context, port=1099)
        ) as server, Host(
            self.test_context, "client", get_client_config(self.test_context, port=1099)
        ) as client:

            self.assertEqual(
                client.exec(
                    ["ping", "-t", "1", "-c", "1", f"{self.test_context.get_ip(1)}"]
                )[2],
                0,
                "Failed to ping from client to server.",
            )

    def test_multiple_servers_iptables_rule(self):
        with Host(
            self.test_context, "server", get_server_config(self.test_context)
        ) as server:
            server.run_stun(
                "extra",
                get_server_config(
                    self.test_context, address_pool="10.180.0.0/24", port=1099
                ),
            )

            masqueraded_subnets = server.get_masqueraded_subnets()

            self.assertEqual(
                len(masqueraded_subnets), 2, "Two MASQUERADE rules should exist."
            )

            self.assertIn(
                f"{self.test_context.get_ip(0)}/24",
                masqueraded_subnets,
                f"{self.test_context.get_ip(0)}/24 should be MASQUERADE-d.",
            )

            self.assertIn(
                "10.180.0.0/24",
                masqueraded_subnets,
                "10.180.0.0/24 should be MASQUERADE-d.",
            )

    def test_multiple_servers_iptables_rule_cleared_properly(self):
        with Host(
            self.test_context, "server", get_server_config(self.test_context)
        ) as server:
            server.run_stun(
                "extra",
                get_server_config(
                    self.test_context, address_pool="10.180.0.0/24", port=1099
                ),
            )

            server.kill_stun("extra")

            server.run_stun(
                "extra",
                get_server_config(
                    self.test_context, address_pool="10.180.0.0/24", port=1099
                ),
            )

            masqueraded_subnets = server.get_masqueraded_subnets()

            self.assertEqual(
                len(masqueraded_subnets), 2, "Two MASQUERADE rules should exist."
            )

            self.assertIn(
                f"{self.test_context.get_ip(0)}/24",
                masqueraded_subnets,
                f"{self.test_context.get_ip(0)}/24 should be MASQUERADE-d.",
            )

            self.assertIn(
                "10.180.0.0/24",
                masqueraded_subnets,
                "10.180.0.0/24 should be MASQUERADE-d.",
            )

    def test_per_user_log_message_with_authentication(self):
        server_config = {"authentication": True, "quotas": {"test-client": 1}}

        with Host(
            self.test_context,
            "server",
            get_server_config(self.test_context, **server_config),
            entry_args=["-v"],
        ) as server, Host(
            self.test_context,
            "client",
            get_client_config(self.test_context, user="test-client"),
            entry_args=["-v"],
        ) as client:

            server_expected_messages = [
                "test-client: Client said hello!",
                "test-client: Client has a quota of 1073741824 bytes.",
                "test-client: Creating a new data pipe.",
            ]

            server_logs = server.logs()

            for message in server_expected_messages:
                self.assertIn(message, server_logs, "Expected log message not present.")

    def test_per_user_log_message_without_authentication(self):
        with Host(
            self.test_context,
            "server",
            get_server_config(self.test_context),
            entry_args=["-v"],
        ) as server, Host(
            self.test_context,
            "client",
            get_client_config(self.test_context),
            entry_args=["-v"],
        ) as client:

            server_expected_messages = [
                re.compile("\d+\.\d+\.\d+\.\d+: Creating a new data pipe.")
            ]

            server_logs = server.logs()

            for message in server_expected_messages:
                self.assertIsNotNone(
                    re.search(message, server_logs), "Expected log message not present."
                )

    def test_masquerade_output_interface_set(self):
        with Host(
            self.test_context,
            "server",
            get_server_config(self.test_context, masquerade_output_interface="eth0"),
        ) as server:
            entries = server.exec(
                ["iptables", "-t", "nat", "-S"], assert_on_failure=True
            )[0].split("\n")

            found = False
            for entry in entries:
                if (
                    f"-A POSTROUTING -s {self.test_context.get_ip(0)}/24 -o eth0 -m comment"
                    in entry
                ):
                    found = True

            self.assertTrue(found, "Expected iptables rule not found.")

    def test_masquerade_output_interface_empty(self):
        with Host(
            self.test_context, "server", get_server_config(self.test_context)
        ) as server:
            entries = server.exec(
                ["iptables", "-t", "nat", "-S"], assert_on_failure=True
            )[0].split("\n")

            found = False
            for entry in entries:
                if (
                    f"-A POSTROUTING -s {self.test_context.get_ip(0)}/24 -m comment"
                    in entry
                ):
                    found = True

            self.assertTrue(found, "Expected iptables rule not found.")

    def test_malformatted_provided_subnet(self):
        with Host(
            self.test_context,
            "server",
            get_server_config(self.test_context),
            entry_args=["-v"],
        ) as server, Host(
            self.test_context,
            "client",
            get_client_config(self.test_context, provided_subnets=["10.179.10.1/24"]),
        ) as client:

            self.assertIn(
                "Sent: config = {", server.logs(), "Did not find expected log entry."
            )

    def test_loss_estimator(self):
        with Host(
            self.test_context,
            "server",
            get_server_config(self.test_context),
            entry_args=["-v"],
        ) as server, Host(
            self.test_context,
            "client",
            get_client_config(self.test_context),
            entry_args=["-v"],
        ) as client:

            client.exec(
                [
                    "ping",
                    "-i",
                    "0.1",
                    "-t",
                    "1",
                    "-c",
                    "10",
                    f"{self.test_context.get_ip(1)}",
                ],
                assert_on_failure=True,
            )
            time.sleep(1)

            self.assertIn(
                "TX loss rate: 0.000000, RX loss rate: 0.000000",
                server.logs(),
                "Expected entry not found in server logs.",
            )
            self.assertIn(
                "TX loss rate: 0.000000, RX loss rate: 0.000000",
                client.logs(),
                "Expected entry not found in client logs.",
            )

    def test_data_pipe_preference_default(self):
        with Host(
            self.test_context,
            "server",
            get_server_config(self.test_context),
            entry_args=["-v"],
        ) as server, Host(
            self.test_context,
            "client",
            get_client_config(self.test_context),
            entry_args=["-v"],
        ) as client:
            time.sleep(1)

            self.assertEqual(
                client.get_messenger_payloads("hello")[0]["data_pipe_preference"],
                ["udp"],
                "Unexpected data_pipe_preference entry in hello message",
            )

            self.assertEqual(
                server.get_messenger_payloads("new_data_pipe")[0]["type"],
                "udp",
                "Unexpected type in new_data_pipe message",
            )

    def test_data_pipe_preference_specified(self):
        with Host(
            self.test_context,
            "server",
            get_server_config(self.test_context),
            entry_args=["-v"],
        ) as server, Host(
            self.test_context,
            "client",
            get_client_config(self.test_context, data_pipe_preference=["tcp", "udp"]),
            entry_args=["-v"],
        ) as client:
            time.sleep(1)

            self.assertEqual(
                client.get_messenger_payloads("hello")[0]["data_pipe_preference"],
                ["tcp", "udp"],
                "Unexpected data_pipe_preference entry in hello message",
            )

            self.assertEqual(
                server.get_messenger_payloads("new_data_pipe")[0]["type"],
                "udp", # TODO: change to TCP once support is added
                "Unexpected type in new_data_pipe message",
            )


if __name__ == "__main__":
    unittest.main()
