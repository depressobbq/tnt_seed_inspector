#!/usr/bin/env bash
set -e

echo "Building Linux CLI tool (tnt_seed_tool)..."
g++ -O3 tnt_seed_cli.cpp -o tnt_seed_tool
strip tnt_seed_tool

echo "Building Windows Standalone GUI (tnt_seed_inspector.exe)..."
x86_64-w64-mingw32-g++ -O3 -mwindows -static tnt_seed_inspector_gui.cpp -o tnt_seed_inspector.exe -lcomctl32
x86_64-w64-mingw32-strip tnt_seed_inspector.exe

echo "Build complete!"
ls -lh tnt_seed_tool tnt_seed_inspector.exe
