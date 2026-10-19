"""CI path-compatible mesh phase; run after material phase in the SAME job."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
if __name__=='__main__':
 from Assets.Unreal.CityApprovedUtilityMeshes import main
 main()
