from pathlib import Path
import traceback, unreal
root=Path(__file__).resolve().parents[1]
try:
    path=root/'Tools/Import-CityEnvironmentArt.py'
    exec(compile(path.read_text(),str(path),'exec'),{'__file__':str(path)})
except Exception:
    (root/'Artifacts/AssetImport/D62-EnvironmentImport.error.txt').write_text(traceback.format_exc())
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
