"""Generate station-upgrade damage while preserving reviewed and unrelated assets."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).absolute().parent))
from CityDamageArt import generate_damage


def main():
    return generate_damage(Path(__file__).absolute().parents[1], scope='station-upgrades')


if __name__ == '__main__':
    main()
