"""Terminate the isolated editor if study scene initialization fails."""
from pathlib import Path
import traceback, unreal
root=Path(__file__).resolve().parents[1]
script=root/'Tools/Capture-CityHomeWindowStudy.py'
try:
    exec(compile(script.read_text(),str(script),'exec'),{'__file__':str(script)})
except Exception:
    command=unreal.SystemLibrary.get_command_line()
    out=root/('Artifacts/D91/Scale/Unreal' if '-D91HomeScale' in command else 'Artifacts/D91/Details/Unreal' if '-D91HomeDetails' in command else 'Artifacts/D91/Unreal');out.mkdir(parents=True,exist_ok=True)
    (out/'Error.txt').write_text(traceback.format_exc())
    unreal.log_error(traceback.format_exc());unreal.SystemLibrary.quit_editor()
