#!/usr/bin/env python3
"""Link the main executable, and with --overlays the 29 overlays, from source with ld.

  relink.py [--overlays] [--jobs N] [--out DIR] [--pad ADDR:N ...] [--all-c]"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from relink.main import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
