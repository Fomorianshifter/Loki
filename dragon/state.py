"""Persistent level, mood, and growth state for Loki.

All thresholds and interaction values are driven by the [dragon] section of
config.toml so you can change growth speed and mood decay without touching code.
"""

from __future__ import annotations

import json
import os
import tempfile
import threading
import time
from pathlib import Path

# ---------------------------------------------------------------------------
# Valid interaction names
# ---------------------------------------------------------------------------
VALID_INTERACTIONS = frozenset({"care", "talk", "play", "feed", "rest"})

# ---------------------------------------------------------------------------
# Defaults (used when config values are absent)
# ---------------------------------------------------------------------------
_DEFAULT_XP_THRESHOLDS = {"hatchling": 5, "juvenile": 25, "adult": 75}
_DEFAULT_XP_PER_INTERACTION = {"care": 2, "talk": 1, "play": 2, "feed": 1, "rest": 1}
_DEFAULT_MOOD_DELTA = {"care": 5, "talk": 3, "play": 6, "feed": 4, "rest": 2}
_DEFAULT_MOOD_THRESHOLDS = {"happy": 80, "content": 55, "sleepy": 30}
_DEFAULT_DECAY = {
    "decay_after_hours": 0.25,
    "decay_hunger_per_hour": 4.0,
    "decay_energy_per_hour": 3.0,
    "decay_mood_per_hour": 2.0,
}

_STAGE_TITLES = {
    "egg": "Dragon Egg",
    "hatchling": "Hatchling",
    "juvenile": "Young Dragon",
    "adult": "Adult Dragon",
}


# ---------------------------------------------------------------------------
# DragonConfig: wraps the [dragon] section of config.toml
# ---------------------------------------------------------------------------
class DragonConfig:
    """Parses and exposes values from the ``[dragon]`` config section.

    All keys are optional; sensible defaults are used for missing values so
    the application never crashes on a minimal or empty ``[dragon]`` block.
    """

    def __init__(self, cfg: dict):
        cfg = cfg or {}
        xp = cfg.get("xp", {}) or {}
        mood = cfg.get("mood", {}) or {}

        # XP thresholds for life-stage transitions
        self.thresholds: dict[str, int] = {
            stage: int(xp.get(stage, _DEFAULT_XP_THRESHOLDS[stage]))
            for stage in ("hatchling", "juvenile", "adult")
        }

        # XP awarded per interaction type
        self.xp_per_interaction: dict[str, int] = {
            k: int(xp.get(k, _DEFAULT_XP_PER_INTERACTION[k]))
            for k in VALID_INTERACTIONS
        }

        # Mood delta per interaction type
        self.mood_delta: dict[str, int] = {
            k: int(mood.get(k, _DEFAULT_MOOD_DELTA[k]))
            for k in VALID_INTERACTIONS
        }

        # Mood name thresholds (0-100)
        self.mood_thresholds: dict[str, int] = {
            label: int(mood.get(label, _DEFAULT_MOOD_THRESHOLDS[label]))
            for label in ("happy", "content", "sleepy")
        }

        # Time-based decay settings
        self.decay_after_hours: float = float(
            mood.get("decay_after_hours", _DEFAULT_DECAY["decay_after_hours"])
        )
        self.decay_hunger_per_hour: float = float(
            mood.get("decay_hunger_per_hour", _DEFAULT_DECAY["decay_hunger_per_hour"])
        )
        self.decay_energy_per_hour: float = float(
            mood.get("decay_energy_per_hour", _DEFAULT_DECAY["decay_energy_per_hour"])
        )
        self.decay_mood_per_hour: float = float(
            mood.get("decay_mood_per_hour", _DEFAULT_DECAY["decay_mood_per_hour"])
        )

        # Persistence settings.
        # Respect XDG_DATA_HOME so the path works in service/rootless environments.
        xdg_data = os.environ.get("XDG_DATA_HOME", "")
        if xdg_data:
            _default_base = Path(xdg_data)
        elif os.environ.get("HOME"):
            _default_base = Path.home() / ".local" / "share"
        else:
            _default_base = Path("/var/lib")
        default_path = str(_default_base / "loki" / "dragon_state.json")
        self.state_path: Path = Path(cfg.get("state_path", default_path)).expanduser()
        self.persist: bool = bool(cfg.get("persist", True))
        self.enabled: bool = bool(cfg.get("enabled", True))


# ---------------------------------------------------------------------------
# DragonState: persistent data container
# ---------------------------------------------------------------------------
class DragonState:
    """Holds Loki's current stats.  Config thresholds are applied at load time
    via :meth:`configure` so the persisted JSON stays config-agnostic.
    """

    # Fields that are serialised to JSON
    FIELDS = ("xp", "mood", "hunger", "energy", "interactions", "last_updated")

    def __init__(
        self,
        xp: int = 0,
        mood: int = 65,
        hunger: int = 20,
        energy: int = 80,
        interactions: int = 0,
        last_updated: float = 0.0,
    ):
        self.xp = xp
        self.mood = mood
        self.hunger = hunger
        self.energy = energy
        self.interactions = interactions
        self.last_updated = last_updated

        # Runtime thresholds (not persisted); set by configure() or defaults
        self._thresholds = dict(_DEFAULT_XP_THRESHOLDS)
        self._mood_thresholds = dict(_DEFAULT_MOOD_THRESHOLDS)
        self._xp_per_interaction = dict(_DEFAULT_XP_PER_INTERACTION)
        self._mood_delta = dict(_DEFAULT_MOOD_DELTA)
        self._decay = dict(_DEFAULT_DECAY)

    # ------------------------------------------------------------------
    # Config injection
    # ------------------------------------------------------------------
    def configure(self, cfg: DragonConfig) -> None:
        """Apply config thresholds without altering persisted fields."""
        self._thresholds = dict(cfg.thresholds)
        self._mood_thresholds = dict(cfg.mood_thresholds)
        self._xp_per_interaction = dict(cfg.xp_per_interaction)
        self._mood_delta = dict(cfg.mood_delta)
        self._decay = {
            "decay_after_hours": cfg.decay_after_hours,
            "decay_hunger_per_hour": cfg.decay_hunger_per_hour,
            "decay_energy_per_hour": cfg.decay_energy_per_hour,
            "decay_mood_per_hour": cfg.decay_mood_per_hour,
        }

    # ------------------------------------------------------------------
    # Computed properties
    # ------------------------------------------------------------------
    @property
    def level(self) -> int:
        """0 = egg, 1 = hatchling, 2 = juvenile, 3 = adult."""
        if self.xp >= self._thresholds["adult"]:
            return 3
        if self.xp >= self._thresholds["juvenile"]:
            return 2
        if self.xp >= self._thresholds["hatchling"]:
            return 1
        return 0

    @property
    def stage(self) -> str:
        """One of: ``egg``, ``hatchling``, ``juvenile``, ``adult``."""
        if self.xp >= self._thresholds["adult"]:
            return "adult"
        if self.xp >= self._thresholds["juvenile"]:
            return "juvenile"
        if self.xp >= self._thresholds["hatchling"]:
            return "hatchling"
        return "egg"

    @property
    def title(self) -> str:
        return _STAGE_TITLES.get(self.stage, "Dragon Egg")

    @property
    def mood_name(self) -> str:
        if self.mood >= self._mood_thresholds["happy"]:
            return "happy"
        if self.mood >= self._mood_thresholds["content"]:
            return "content"
        if self.mood >= self._mood_thresholds["sleepy"]:
            return "sleepy"
        return "grumpy"

    # ------------------------------------------------------------------
    # Mutations
    # ------------------------------------------------------------------
    def apply_time_decay(self, now: float | None = None) -> None:
        """Reduce needs when Loki has been left alone.  Safe to call with
        ``last_updated == 0`` (first-run guard)."""
        now = time.time() if now is None else now

        if not self.last_updated:
            self.last_updated = now
            return

        hours_away = max(0.0, (now - self.last_updated) / 3600.0)
        if hours_away < self._decay["decay_after_hours"]:
            return

        self.hunger = min(100, self.hunger + int(hours_away * self._decay["decay_hunger_per_hour"]))
        self.energy = max(0, self.energy - int(hours_away * self._decay["decay_energy_per_hour"]))
        self.mood = max(0, self.mood - int(hours_away * self._decay["decay_mood_per_hour"]))
        self.last_updated = now

    def interact(self, kind: str) -> dict:
        """Record a healthy interaction and award XP / mood progress.

        Returns a summary dict describing the result.
        Raises :exc:`ValueError` for unknown interaction names.
        """
        if kind not in VALID_INTERACTIONS:
            raise ValueError(f"Unknown interaction: {kind!r}")

        self.apply_time_decay()
        stage_before = self.stage

        self.xp += self._xp_per_interaction.get(kind, 1)
        self.mood = min(100, self.mood + self._mood_delta.get(kind, 0))
        self.interactions += 1

        if kind == "feed":
            self.hunger = max(0, self.hunger - 25)
        elif kind == "play":
            self.energy = max(0, self.energy - 10)
        elif kind == "rest":
            self.energy = min(100, self.energy + 20)

        self.last_updated = time.time()

        return {
            "interaction": kind,
            "stage_before": stage_before,
            "stage_after": self.stage,
            "level": self.level,
            "xp": self.xp,
            "mood": self.mood_name,
        }

    # ------------------------------------------------------------------
    # Serialisation
    # ------------------------------------------------------------------
    def to_dict(self) -> dict:
        return {k: getattr(self, k) for k in self.FIELDS}

    @classmethod
    def from_dict(cls, data: dict) -> "DragonState":
        return cls(**{k: data[k] for k in cls.FIELDS if k in data})


# ---------------------------------------------------------------------------
# DragonStateStore: atomic JSON persistence
# ---------------------------------------------------------------------------
class DragonStateStore:
    """Reads and writes :class:`DragonState` as JSON without requiring a DB.

    Set ``persist=False`` to disable file I/O (useful for tests and headless
    preview runs).
    """

    def __init__(self, path: str | Path, persist: bool = True):
        self.path = Path(path).expanduser()
        self.persist = persist
        self._save_lock = threading.Lock()

    def load(self, cfg: DragonConfig | None = None) -> DragonState:
        """Load state from disk, apply config thresholds, then time-decay."""
        state: DragonState | None = None

        if self.persist and self.path.exists():
            try:
                state = DragonState.from_dict(json.loads(self.path.read_text()))
            except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError):
                state = None

        if state is None:
            state = DragonState(last_updated=time.time())
            if self.persist:
                self.save(state)

        if cfg is not None:
            state.configure(cfg)

        state.apply_time_decay()
        return state

    def save(self, state: DragonState) -> None:
        """Atomically write state to disk.  No-op when ``persist=False``."""
        if not self.persist:
            return
        with self._save_lock:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            fd, temporary_path = tempfile.mkstemp(
                dir=self.path.parent, prefix=f".{self.path.name}.", suffix=".tmp"
            )
            try:
                with os.fdopen(fd, "w", encoding="utf-8") as temporary:
                    temporary.write(json.dumps(state.to_dict(), indent=2) + "\n")
                os.replace(temporary_path, self.path)
            except Exception:
                if os.path.exists(temporary_path):
                    os.unlink(temporary_path)
                raise
