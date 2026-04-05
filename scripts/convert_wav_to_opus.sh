#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dry_run=0
delete_originals=0
force=0
bgm_bitrate="128k"
voice_bitrate="48k"

usage() {
  cat <<'EOF'
Usage: scripts/convert_wav_to_opus.sh [options]

Options:
  --dry-run            Print planned conversions without writing files.
  --delete-originals   Remove each source .wav after a successful conversion.
  --force              Re-encode even if the .opus file already exists.
  --bgm-bitrate RATE   Opus bitrate for BGM buckets. Default: 128k
  --voice-bitrate RATE Opus bitrate for voice buckets. Default: 48k
  -h, --help           Show this help text.

Default buckets:
  assets/ui/classics
  assets/combat/bgm
  assets/vn/bgm
  assets/vn/voices
  assets/combat/voices
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dry-run)
      dry_run=1
      shift
      ;;
    --delete-originals)
      delete_originals=1
      shift
      ;;
    --force)
      force=1
      shift
      ;;
    --bgm-bitrate)
      bgm_bitrate="${2:?missing value for --bgm-bitrate}"
      shift 2
      ;;
    --voice-bitrate)
      voice_bitrate="${2:?missing value for --voice-bitrate}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg is required for conversion." >&2
  exit 1
fi

target_dirs=(
  "assets/ui/classics"
  "assets/combat/bgm"
  "assets/vn/bgm"
  "assets/vn/voices"
  "assets/combat/voices"
)

pick_bitrate() {
  local path="$1"
  case "$path" in
    */ui/classics/*|*/combat/bgm/*|*/vn/bgm/*)
      printf '%s\n' "$bgm_bitrate"
      ;;
    */vn/voices/*|*/combat/voices/*)
      printf '%s\n' "$voice_bitrate"
      ;;
    *)
      return 1
      ;;
  esac
}

converted_count=0
skipped_count=0

for relative_dir in "${target_dirs[@]}"; do
  absolute_dir="$repo_root/$relative_dir"
  if [[ ! -d "$absolute_dir" ]]; then
    continue
  fi

  while IFS= read -r -d '' wav_file; do
    bitrate="$(pick_bitrate "$wav_file")" || {
      ((skipped_count+=1))
      continue
    }

    opus_file="${wav_file%.*}.opus"
    if [[ -f "$opus_file" && "$force" -eq 0 ]]; then
      echo "skip  $wav_file -> $opus_file (already exists)"
      ((skipped_count+=1))
      continue
    fi

    echo "encode $wav_file -> $opus_file [$bitrate]"
    if [[ "$dry_run" -eq 1 ]]; then
      ((converted_count+=1))
      continue
    fi

    ffmpeg -hide_banner -loglevel error -y \
      -nostdin \
      -i "$wav_file" \
      -c:a libopus \
      -b:a "$bitrate" \
      -vbr on \
      -compression_level 10 \
      "$opus_file"

    if [[ "$delete_originals" -eq 1 ]]; then
      rm -f "$wav_file"
    fi
    ((converted_count+=1))
  done < <(find "$absolute_dir" -type f -iname '*.wav' -print0 | sort -z)
done

echo "done  converted=$converted_count skipped=$skipped_count dry_run=$dry_run delete_originals=$delete_originals"
