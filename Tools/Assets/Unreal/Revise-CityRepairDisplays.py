"""Update only the two reviewed repair texture/material products under the CI lock."""

import sys
from pathlib import Path
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from CityToolPaths import repository_root
import runpy
from pathlib import Path
def main():
    runpy.run_path(str(Path(__file__).with_name('Import-CityEditorialArt.py')),
                  init_globals={'repairs_only':True}, run_name='__main__')

if __name__ == '__main__':
    main()
