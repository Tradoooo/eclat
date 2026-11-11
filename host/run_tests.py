# SPDX-License-Identifier: Apache-2.0
"""Éclat — lance les tests unitaires Python du jumeau (lot L3).

Usage : python3 host/run_tests.py
Découvre les `*_test.py` / `test_*.py` dans host/ et les exécute. Retourne 0 si tout est vert.
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def main():
    loader = unittest.TestLoader()
    suite = loader.discover(HERE, pattern="test_*.py")
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())