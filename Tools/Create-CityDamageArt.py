"""Generate damage derivatives using the shared recipe and existing CLI flags."""
from pathlib import Path
import argparse
import sys
sys.path.insert(0, str(Path(__file__).absolute().parent))
from CityDamageArt import generate_damage


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument('--SchoolOnly', action='store_true')
    scope.add_argument('--ParksOnly', action='store_true')
    arguments = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [arg for arg in sys.argv[1:] if arg in ('--SchoolOnly', '--ParksOnly')]
    args = parser.parse_args(arguments)
    selected = 'school' if args.SchoolOnly else 'parks' if args.ParksOnly else 'all'
    return generate_damage(Path(__file__).absolute().parents[1], scope=selected)


if __name__ == '__main__':
    main()
