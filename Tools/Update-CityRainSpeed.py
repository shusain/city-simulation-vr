"""Regenerate only the native rain emitter after changing RainLifetime.

Run through UnrealEditor-Cmd -run=pythonscript -Script=<absolute path>.
Snow, sprite masks/materials, maps and player saves are preserved.
"""
from pathlib import Path
import hashlib
import unreal

root = Path(__file__).resolve().parents[1]
snow = root / 'CitySimVR/Content/CitySim/Art/Weather/P_CitySnow.uasset'
before = hashlib.sha256(snow.read_bytes()).hexdigest()
if not unreal.CityWeatherParticleComponent.create_weather_assets(True):
    raise RuntimeError('Rain asset update failed')
assert hashlib.sha256(snow.read_bytes()).hexdigest() == before, 'Snow asset changed'
print('CITYSIM_RAIN_SPEED_UPDATED SnowUnchanged=1')
