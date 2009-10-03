#! /usr/bin/env bash

for f in *; do
  if test -d "$f"; then
    for pot in *.pot; do
      test -f "$f/kdenlive.po" && msgmerge -U "$f/kdenlive.po" "$pot";
      echo "processing $f/kdenlive.po";
    done
  fi
done
