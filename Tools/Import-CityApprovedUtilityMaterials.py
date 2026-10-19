"""CI path-compatible material phase. Native execution is owned by #125."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
if __name__=='__main__':
 from Assets.Unreal.CityApprovedUtilityMaterials import main
 main()
