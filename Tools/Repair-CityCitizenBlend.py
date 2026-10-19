"""Explicit CI entry point; implementation lives in Assets/Unreal."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parent/'Assets/Unreal/Repair-CityCitizenBlend.py'),run_name='__main__')
