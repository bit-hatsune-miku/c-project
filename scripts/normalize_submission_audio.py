#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
VOICE_ROOTS = [
    REPO_ROOT / "assets" / "combat" / "voices",
    REPO_ROOT / "assets" / "vn" / "voices",
]
BGM_ROOTS = [
    REPO_ROOT / "assets" / "combat" / "bgm",
]
VOICE_EXTS = {".opus", ".wav", ".mp3", ".m4a"}
BGM_EXTS = {".opus", ".wav"}
VOICE_TARGET = {
    "I": -18.0,
    "TP": -1.5,
    "LRA": 7.0,
}
BGM_TARGET = {
    "I": -20.0,
    "TP": -1.5,
    "LRA": 11.0,
}
SILENCE_FILTER = (
    "silenceremove="
    "start_periods=1:"
    "start_duration=0.05:"
    "start_threshold=-35dB:"
    "start_silence=0.02"
)


@dataclass(frozen=True)
class AudioJob:
    path: Path
    kind: str


def list_audio_jobs() -> list[AudioJob]:
    jobs: list[AudioJob] = []
    for root in VOICE_ROOTS:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*")):
            if path.is_file() and path.suffix.lower() in VOICE_EXTS:
                jobs.append(AudioJob(path=path, kind="voice"))
    for root in BGM_ROOTS:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*")):
            if path.is_file() and path.suffix.lower() in BGM_EXTS:
                jobs.append(AudioJob(path=path, kind="battle_bgm"))
    return jobs


def ffmpeg_encoder_args(path: Path) -> list[str]:
    suffix = path.suffix.lower()
    if suffix == ".opus":
        return ["-c:a", "libopus", "-b:a", "96k", "-vbr", "on", "-application", "audio"]
    if suffix == ".wav":
        return ["-c:a", "pcm_s16le"]
    if suffix == ".mp3":
        return ["-c:a", "libmp3lame", "-q:a", "2"]
    if suffix == ".m4a":
        return ["-c:a", "aac", "-b:a", "192k"]
    raise ValueError(f"Unsupported extension: {path.suffix}")


def filter_graph_for(job: AudioJob) -> str:
    target = VOICE_TARGET if job.kind == "voice" else BGM_TARGET
    filters: list[str] = []
    if job.kind == "voice":
        filters.append(SILENCE_FILTER)
        filters.append(
            "loudnorm="
            f"I={target['I']}:"
            f"TP={target['TP']}:"
            f"LRA={target['LRA']}:"
            "dual_mono=true"
        )
    else:
        filters.append(
            "loudnorm="
            f"I={target['I']}:"
            f"TP={target['TP']}:"
            f"LRA={target['LRA']}"
        )
    return ",".join(filters)


def backup_root() -> Path:
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    root = Path("/tmp") / f"bit-audio-backup-{timestamp}"
    root.mkdir(parents=True, exist_ok=False)
    return root


def copy_backup(job: AudioJob, backup_dir: Path) -> Path:
    relative = job.path.relative_to(REPO_ROOT)
    destination = backup_dir / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(job.path, destination)
    return destination


def probe_duration(path: Path) -> float:
    result = subprocess.run(
        [
            "ffprobe",
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=noprint_wrappers=1:nokey=1",
            str(path),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    return float(result.stdout.strip())


def process_file(job: AudioJob) -> dict:
    source = job.path
    duration_before = probe_duration(source)
    fd, tmp_path_str = tempfile.mkstemp(
        prefix=f".{source.stem}.normalize-",
        suffix=source.suffix,
        dir=source.parent,
    )
    os.close(fd)
    tmp_output = Path(tmp_path_str)
    command = [
        "ffmpeg",
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(source),
        "-map",
        "0:a:0",
        "-af",
        filter_graph_for(job),
        *ffmpeg_encoder_args(source),
        str(tmp_output),
    ]
    subprocess.run(command, check=True)
    duration_after = probe_duration(tmp_output)
    if duration_after <= 0.0:
        raise RuntimeError(f"Processed file has invalid duration: {tmp_output}")
    os.replace(tmp_output, source)
    return {
        "path": source.relative_to(REPO_ROOT).as_posix(),
        "kind": job.kind,
        "duration_before": round(duration_before, 6),
        "duration_after": round(duration_after, 6),
    }


def main() -> int:
    jobs = list_audio_jobs()
    if not jobs:
        print("No audio jobs found.")
        return 0

    backup_dir = backup_root()
    manifest: list[dict] = []
    report: list[dict] = []

    for job in jobs:
        backup_path = copy_backup(job, backup_dir)
        manifest.append(
            {
                "path": job.path.relative_to(REPO_ROOT).as_posix(),
                "kind": job.kind,
                "backup": backup_path.as_posix(),
            }
        )

    manifest_path = backup_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2))

    for job in jobs:
        result = process_file(job)
        report.append(result)

    report_path = backup_dir / "report.json"
    report_path.write_text(json.dumps(report, indent=2))

    summary = {
        "backup_dir": backup_dir.as_posix(),
        "processed_files": len(report),
        "voice_files": sum(1 for item in report if item["kind"] == "voice"),
        "battle_bgm_files": sum(1 for item in report if item["kind"] == "battle_bgm"),
        "manifest": manifest_path.as_posix(),
        "report": report_path.as_posix(),
    }
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
