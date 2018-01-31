#!/usr/bin/env python2.7

import pandas as pd
import os
import sys
import json
import glob

if len(sys.argv) != 3:
    print "Usage: %s <glob> <output.csv>"
    sys.exit(1)

paths = sys.argv[1]
output = sys.argv[2]

rows = []

for path in glob.glob(paths):
    print path, ":",
    num_records = 0
    with open(path) as f:
        for line in f.readlines():
            try:
                rows.append(json.loads(line))
                num_records += 1
            except ValueError:
                pass
    print num_records, "records"

data = pd.DataFrame.from_records(rows)
data = data.fillna(method="ffill")
data.to_csv(output)
print "Saved to " + output
