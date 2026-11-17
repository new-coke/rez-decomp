#!/usr/bin/env python3
"""Write a relink build into copies of REZ.CVM and the disc image.

  build_disc.py --from build/relink/ps2-us-1.01/exact [--out DIR]"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from relink.disc import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
