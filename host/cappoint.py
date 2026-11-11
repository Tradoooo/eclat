# SPDX-License-Identifier: Apache-2.0
"""Éclat — `cappoint <policy.toml>` : valide une politique (lot L3, ECL-02/ECL-03).

Mêmes conventions que le C (CONTRACT.md §6) :
- politique valide  → stdout « OK », code de sortie 0 ;
- politique invalide → stderr « ERROR:<field>:<message> », code de sortie 2.
"""
import sys

import capsim


def main(argv):
    if len(argv) != 2:
        sys.stderr.write("Usage: cappoint.py <policy.toml>\n")
        return 2
    try:
        with open(argv[1], "rb") as fp:
            data = fp.read()
    except OSError as exc:
        sys.stderr.write("ERROR:policy:%s\n" % exc)
        return 2
    policy, err = capsim.parse_policy(data)
    if err is not None:
        sys.stderr.write("ERROR:%s:%s\n" % (err[0], err[1]))
        return 2
    sys.stdout.write("OK\n")
    sys.stdout.write("# module=%s allows=%d denies=%d\n"
                     % (policy["name"], len(policy["allows"]), len(policy["denies"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))