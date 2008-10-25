#! /usr/bin/env bash

for f in *; do
  if test -d "$f"; then
    for pot in *.pot; do
      test -f "$f/$f.po" && msgmerge -U "$f/$f.po" "$pot";
      echo "processing $f/$f.po";
    done
  fi
done
