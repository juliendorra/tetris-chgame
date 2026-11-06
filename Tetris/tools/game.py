"""Rules, real audio backend and button-level simulator checks."""
TESTS = {"rules": {"sources": ["tools/tests/rules.cpp", "Game.cpp"], "args": "none",
                   "extra_flags": ["-fsanitize=undefined", "-fno-sanitize-recover=all"]},
         "output": {"sources": ["tools/tests/output_test.cpp"], "args": "none",
                    "extra_flags": ["-fsanitize=undefined", "-fno-sanitize-recover=all"]}}
EXTRA_TEST_SCRIPTS = ["tools/verify_audio.py"]
SIM_TESTS = ["tools/verify_sim.py", "tools/verify_dmg.py", "tools/verify_clear.py"]
