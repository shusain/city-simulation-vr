"""Compatibility entry point; use Tools/Audio/Soundscape/Import-CityAudioSources.py."""
from pathlib import Path
import runpy
_entry = Path(__file__).resolve().parent / 'Audio/Soundscape/Import-CityAudioSources.py'
globals().update(runpy.run_path(str(_entry), init_globals=globals(), run_name=__name__))
