#!/usr/bin/env python3
"""Validated UTC epoch for reproducible ISO metadata."""
import datetime
import os
import sys
try:
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH', '1794960000'))
except ValueError:
    raise SystemExit('SOURCE_DATE_EPOCH must be an integer')
if not 315532800 <= epoch <= 4354819199:
    raise SystemExit('SOURCE_DATE_EPOCH must represent 1980 through 2107 UTC')
value = datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc)
print(value.strftime('%Y%m%d%H%M%S') + '00')
