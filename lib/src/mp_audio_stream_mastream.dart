// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import '../mp_audio_stream.dart';
import '../gen/mp_audio_stream.dart';

/// Control class for AudioStream on "not" web platform. Use `getAudioStream()` to get its instance.
class AudioStreamImpl implements AudioStream {
  late AudioStreamNative ffiModule;

  late ReceivePort nativeRequests;
  late int nativePort;

  @override
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

    ffiModule = AudioStreamNative(dynLib);

    final res = ffiModule.init_dart_api_dl(NativeApi.initializeApiDLData);
    if (res != 0) {
      throw Exception("Failed to initialize dynamic dart api in audio module");
    }

    nativeRequests = ReceivePort();

    nativePort = nativeRequests.sendPort.nativePort;
    ffiModule.init_port(nativePort);

    print("INITIALIZED AUDIO SYSTEM");

    return ffiModule.ma_stream_init(bufferMilliSec * sampleRate ~/ 1000,
        waitingBufferMilliSec * sampleRate ~/ 1000, channels, sampleRate);
  }

  @override
  int push(Float32List buf, int userId) {
    final ffiBuf = calloc<Float>(buf.length);
    for (int i = 0; i < buf.length; i++) {
      ffiBuf[i] = buf[i];
    }
    final result = ffiModule.ma_stream_push(ffiBuf, buf.length, userId);
    calloc.free(ffiBuf);
    return result;
  }

  @override
  Float32List getMicData(int length) {
    final pointer = ffiModule.get_mic_data(length);
    // Copy the list
    final floatArray = Float32List.fromList(pointer.asTypedList(length));
    //calloc.free(pointer);
    return floatArray;
  }

/*   @override
  AudioStreamStat stat(int userId) {
    return AudioStreamStat(
        full: _statFullCountFfi(), exhaust: _statExhaustCountFfi(), userId: 0);
  } */

  @override
  void uninit() {
    ffiModule.ma_stream_uninit();
  }

  @override
  void resume() {}

/*   @override
  void resetStat() {
    _statResetFfi();
  } */

  @override
  void removeBuffer(int userId) {
    ffiModule.parlo_remove_user(userId);
  }

  @override
  bool isReady(int length) {
    return ffiModule.is_mic_ready(length);
  }

  @override
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

    sub = nativeRequests.listen((message) {
      Uint8List data = message;
      print(data);
/*       if (started) {
        controller.add(getMicData(960));
      } */
    });

    return controller.stream;
  }

  @override
  ReceivePort getPort() {
    return nativeRequests;
  }

  @override
  int opusPush(Uint8List buf, int userId) {
    final ffiBuf = calloc<Uint8>(buf.length);
    for (int i = 0; i < buf.length; i++) {
      ffiBuf[i] = buf[i];
    }
    final result = ffiModule.push_opus(ffiBuf, buf.length, userId);
    calloc.free(ffiBuf);
    return result;
  }
}
