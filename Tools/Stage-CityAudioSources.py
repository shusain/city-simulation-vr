"""Compatibility entry point; use Tools/Audio/Soundscape/Stage-CityAudioSources.py."""
from pathlib import Path
import runpy
_entry = Path(__file__).resolve().parent / 'Audio/Soundscape/Stage-CityAudioSources.py'
globals().update(runpy.run_path(str(_entry), init_globals=globals(), run_name=__name__))
