"""CI-compatible entry point; canonical nuclear importer lives under Assets."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).resolve().parent/'Assets/Unreal/Import-CityNuclearFacility.py'),run_name='__main__')
