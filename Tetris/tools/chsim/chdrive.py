"""Drive the game through the shared CHGame simulator/device protocol."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'CHGame/tools/chsim'))
from chdrivelib import Driver, main
IDENT = 'TETRIS'

if __name__ == '__main__':
    main(Driver, ident=IDENT)
