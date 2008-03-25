#! /usr/bin/env bash

for f in *; do
  if test -d "$f"; then
    for pot in *.pot; do
      test -f "$f/${pot%*.*}.po" && msgmerge -U "$f/${pot%*.*}.po" "$pot";
    done
  fi
done
