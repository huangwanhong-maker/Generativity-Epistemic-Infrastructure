"""List installed module metadata; optionally activate one explicitly trusted factory.

python module_discovery.py
python module_discovery.py --activate org.gr.analysis.relational
"""

import argparse
import json
from gr_os.modules import discover

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--activate", help="Import this trusted package's manifest factory")
args = parser.parse_args()
entries = discover()
if args.activate:
    entry = next((item for item in entries if item.id == args.activate), None)
    if entry is None:
        parser.error("selected module ID is not installed")
    print(json.dumps(entry.activate().describe(), indent=2))
else:
    print(json.dumps([{"id": item.id, "distribution": item.distribution,
                       "version": item.version, "reference": item.reference}
                      for item in entries], indent=2))
