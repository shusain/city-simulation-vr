"""Explicit CI entry point; canonical implementation lives in Audio/Soundscape."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parent/'Audio/Soundscape/Repair-CitySoundscapeReferences.py'),run_name='__main__')
