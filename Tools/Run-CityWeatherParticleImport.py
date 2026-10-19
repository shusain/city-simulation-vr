"""Run the separate native weather asset authoring pass and exit the editor."""
from pathlib import Path
import traceback, unreal
root = Path(__file__).resolve().parents[1]
(root / 'Artifacts/D62-WeatherParticleImport.error.txt').unlink(missing_ok=True)
try:
    script = root / 'Tools/Create-CityWeatherParticles.py'
    exec(compile(script.read_text(), str(script), 'exec'), {'__file__': str(script), '__name__': '__main__'})
except Exception:
    (root / 'Artifacts/D62-WeatherParticleImport.error.txt').write_text(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
