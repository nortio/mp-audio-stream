// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

/// A multi-platform audio stream output library for real-time generated wave data
library audio_stream;

import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'gen/mp_audio_stream.dart';

/// Control class for AudioStream on "not" web platform. Use `getAudioStream()` to get its instance.
class AudioStream {
  late AudioStreamNative _ffiModule;

  late ReceivePort _nativeRequests;
  late ReceivePort _levelPort;
  late int nativePort;

  /// Initializes an audio stream and starts to play. Returns 0 then scucess.
  /// Calling more than once makes a new AudioStream, the previous device will be `uninit`ed.
  int init(
      {int bufferMilliSec = 3000,
      int waitingBufferMilliSec = 100,
      int channels = 1,
      int sampleRate = 48000}) {
    final dynLib = (Platform.isLinux || Platform.isAndroid)
        ? DynamicLibrary.open("libmp_audio_stream.so")
        : Platform.isWindows
            ? DynamicLibrary.open("mp_audio_stream.dll")
            : (Platform.isMacOS || Platform.isIOS)
                ? DynamicLibrary.executable()
                : DynamicLibrary.executable();

    _ffiModule = AudioStreamNative(dynLib);

    final res = _ffiModule.init_dart_api_dl(NativeApi.initializeApiDLData);
    if (res != 0) {
      throw Exception("Failed to initialize dynamic dart api in audio module");
    }

    _nativeRequests = ReceivePort();
    _levelPort = ReceivePort();

    nativePort = _nativeRequests.sendPort.nativePort;
    final levelPortSend = _levelPort.sendPort.nativePort;
    _ffiModule.init_port(nativePort, levelPortSend);

    return _ffiModule.ma_stream_init(bufferMilliSec * sampleRate ~/ 1000,
        waitingBufferMilliSec * sampleRate ~/ 1000, channels, sampleRate);
  }

  /// Resumes audio stream.
  /// For web platform, you should call this from some user-action to activate `AudioContext`.
  /// Ignored on platforms other than web, but recommended to call this to keep multi-platform ready.
  void resume() {}

  /// Pushes wave data (float32, -1.0 to 1.0) into audio stream. When buffer is full, the input is ignored.
  int push(Float32List buf, int userId) {
    final ffiBuf = calloc<Float>(buf.length);
    for (int i = 0; i < buf.length; i++) {
      ffiBuf[i] = buf[i];
    }
    final result = _ffiModule.ma_stream_push(ffiBuf, buf.length, userId);
    calloc.free(ffiBuf);
    return result;
  }

  void uninit() {
    _ffiModule.ma_stream_uninit();
  }

  void removeBuffer(int userId) {
    _ffiModule.parlo_remove_user(userId);
  }

  Stream<Float32List> inputStream() {
    bool started = false;
    late StreamSubscription<dynamic> sub;

    void stopSub() {
      started = false;
      sub.cancel();
    }

    final controller = StreamController<Float32List>(
        onListen: () {
          started = true;
        },
        onPause: () {
          started = false;
        },
        onResume: () {
          started = true;
        },
        onCancel: stopSub);

    sub = _nativeRequests.listen((message) {
      Uint8List data = message;
/*       if (started) {
        controller.add(getMicData(960));
      } */
    });

    return controller.stream;
  }

  ReceivePort getPort() {
    return _nativeRequests;
  }

  ReceivePort getLevelPort() {
    return _levelPort;
  }


  int opusPush(Uint8List buf, int userId) {
    final ffiBuf = calloc<Uint8>(buf.length);
    for (int i = 0; i < buf.length; i++) {
      ffiBuf[i] = buf[i];
    }
    final result = _ffiModule.push_opus(ffiBuf, buf.length, userId);
    calloc.free(ffiBuf);
    return result;
  }

  void setThreshold(double t) {
    _ffiModule.set_threshold(t);
  }
}


/// Returns an `AudioStream` instance for running platform (web/others)
AudioStream getAudioStream() => AudioStream();
