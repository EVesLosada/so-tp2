#!/bin/bash

IMAGE="Image/x64BareBonesImage.qcow2"

if [ ! -f "$IMAGE" ]; then
  IMAGE="Image/x64BareBonesImage.vmdk"
fi

if [ ! -f "$IMAGE" ]; then
  echo "No se encontro imagen booteable en Image/"
  exit 1
fi

AUDIO_BACKEND=""

if qemu-system-x86_64 -audiodev help 2>&1 | grep -q "coreaudio"; then
  AUDIO_BACKEND="coreaudio"
elif qemu-system-x86_64 -audiodev help 2>&1 | grep -q "pa"; then
  AUDIO_BACKEND="pa"
elif qemu-system-x86_64 -audiodev help 2>&1 | grep -q "pipewire"; then
  AUDIO_BACKEND="pipewire"
elif qemu-system-x86_64 -audiodev help 2>&1 | grep -q "sdl"; then
  AUDIO_BACKEND="sdl"
fi

if [ -n "$AUDIO_BACKEND" ]; then
  qemu-system-x86_64 \
    -hda "$IMAGE" \
    -m 512 \
    -audiodev "$AUDIO_BACKEND",id=snd0 \
    -machine pcspk-audiodev=snd0
else
  echo "No se encontro backend de audio compatible. Ejecutando sin sonido."
  qemu-system-x86_64 \
    -hda "$IMAGE" \
    -m 512
fi