"""PlatformIO pre-script: regenerate src/platform/srush_game.c from game/main.c before every build."""
import subprocess, sys
Import("env")  # type: ignore[name-defined]  # provided by PlatformIO
subprocess.check_call([sys.executable, env.subst("$PROJECT_DIR/tools/sync_game.py")])  # type: ignore[name-defined]
