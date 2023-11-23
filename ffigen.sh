#!/usr/bin/env bash
# ffigen, change include path to standard include dir
# https://github.com/dart-lang/ffigen/issues/257
dart run ffigen --compiler-opts "-I/usr/lib/clang/16/include"