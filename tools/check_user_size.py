#!/usr/bin/env python3
"""Fail before embedding a native image that cannot fit in a VFS file."""
from pathlib import Path
import re
import sys
maximum=int(re.search(r"#define FS_FILE_MAX (\d+)u",(Path(__file__).resolve().parents[1]/"kernel/fs/vfs.h").read_text())[1])
image=Path(sys.argv[1])
if image.stat().st_size>maximum:
    raise SystemExit(f"{image}: {image.stat().st_size} bytes exceeds RAM VFS file maximum {maximum}; reduce the native image before release")
