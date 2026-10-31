import os
import shutil
import subprocess
import sys

RIAG_ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
# Datasets can only be copied from the root since copy.schema contains relative paths
os.chdir(RIAG_ROOT)

# Define the build type from input
if len(sys.argv) > 1 and sys.argv[1].lower() == "release":
    build_type = "release"
else:
    build_type = "relwithdebinfo"

# Change the current working directory
if os.path.exists(f"{RIAG_ROOT}/dataset/databases/tinysnb"):
    shutil.rmtree(f"{RIAG_ROOT}/dataset/databases/tinysnb")
if sys.platform == "win32":
    riag_shell_path = f"{RIAG_ROOT}/build/{build_type}/src/riag_shell"
else:
    riag_shell_path = f"{RIAG_ROOT}/build/{build_type}/tools/shell/riag"
subprocess.check_call(
    [
        "python3",
        f"{RIAG_ROOT}/benchmark/serializer.py",
        "TinySNB",
        f"{RIAG_ROOT}/dataset/tinysnb",
        f"{RIAG_ROOT}/dataset/databases/tinysnb",
        "--single-thread",
        "--riag-shell",
        riag_shell_path,
    ]
)
