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

typedef _MAInitFunc = Int Function(Int64, Int64, Int64, Int64);
typedef _MAInit = int Function(int, int, int, int);

typedef _MAPushFunc = Int Function(Pointer<Float>, Int64, Int64);
typedef _MAPush = int Function(Pointer<Float>, int, int);

typedef _MAVoidFunc = Void Function();
typedef _MAVoid = void Function();

/* typedef _MAIntFunc = Int Function();
typedef _MAInt = int Function(); */

typedef _RemoveUserFunc = Void Function(Int64);
typedef _RemoveUser = void Function(int);

typedef _GetMicDataFunc = Pointer<Float> Function(Int64);
typedef _GetMicData = Pointer<Float> Function(int);

typedef _IsMicReadyFunc = Bool Function(Int64);
typedef _IsMicReady = bool Function(int);

typedef _InitDartApiFunc = IntPtr Function(Pointer<Void>);
typedef _InitDartApi = int Function(Pointer<Void>);

/// Control class for AudioStream on "not" web platform. Use `getAudioStream()` to get its instance.
class AudioStreamImpl implements AudioStream {
  late _MAPush _pushFfi;
  late _MAVoid _uninitFfi;
/*   late _MAInt _statExhaustCountFfi;
  late _MAInt _statFullCountFfi;
  late _MAVoid _statResetFfi; */
  late _RemoveUser _removeUser;
  late _GetMicData _getMicData;

  late _IsMicReady _isMicReady;
  late _InitDartApi _initDartApi;

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

    final initFfi = dynLib
        .lookup<NativeFunction<_MAInitFunc>>("ma_stream_init")
        .asFunction<_MAInit>();

    _pushFfi = dynLib
        .lookup<NativeFunction<_MAPushFunc>>("ma_stream_push")
        .asFunction<_MAPush>();

    _uninitFfi = dynLib
        .lookup<NativeFunction<_MAVoidFunc>>("ma_stream_uninit")
        .asFunction<_MAVoid>();

/*     _statExhaustCountFfi = dynLib
        .lookup<NativeFunction<_MAIntFunc>>("ma_stream_stat_exhaust_count")
        .asFunction<_MAInt>();

    _statFullCountFfi = dynLib
        .lookup<NativeFunction<_MAIntFunc>>("ma_stream_stat_full_count")
        .asFunction<_MAInt>();

    _statResetFfi = dynLib
        .lookup<NativeFunction<_MAVoidFunc>>("ma_stream_stat_reset")
        .asFunction<_MAVoid>(); */

    _removeUser = dynLib
        .lookup<NativeFunction<_RemoveUserFunc>>("parlo_remove_user")
        .asFunction<_RemoveUser>();

    _getMicData = dynLib
        .lookup<NativeFunction<_GetMicDataFunc>>("get_mic_data")
        .asFunction<_GetMicData>();

    _isMicReady = dynLib
        .lookup<NativeFunction<_IsMicReadyFunc>>("is_mic_ready")
        .asFunction<_IsMicReady>();

    _initDartApi = dynLib
        .lookup<NativeFunction<_InitDartApiFunc>>("init_dart_api_dl")
        .asFunction<_InitDartApi>();

    final res = _initDartApi(NativeApi.initializeApiDLData);
    if (res != 0) {
      throw Exception("Failed to initialize dynamic dart api in audio module");
    }

    nativeRequests = ReceivePort();

    nativePort = nativeRequests.sendPort.nativePort;

    final initPort = dynLib
        .lookupFunction<Void Function(Int64), void Function(int)>("init_port");
    initPort(nativePort);

    print("INITIALIZED AUDIO SYSTEM");

    return initFfi(bufferMilliSec * sampleRate ~/ 1000,
        waitingBufferMilliSec * sampleRate ~/ 1000, channels, sampleRate);
  }

  @override
  int push(Float32List buf, int userId) {
    final ffiBuf = calloc<Float>(buf.length);
    for (int i = 0; i < buf.length; i++) {
      ffiBuf[i] = buf[i];
    }
    final result = _pushFfi(ffiBuf, buf.length, userId);
    calloc.free(ffiBuf);
    return result;
  }

  @override
  Float32List getMicData(int length) {
    final pointer = _getMicData(length);
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
    _uninitFfi();
  }

  @override
  void resume() {}

/*   @override
  void resetStat() {
    _statResetFfi();
  } */

  @override
  void removeBuffer(int userId) {
    _removeUser(userId);
  }

  @override
  bool isReady(int length) {
    return _isMicReady(length);
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
      if (started) {
        controller.add(getMicData(960));
      }
    });

    return controller.stream;
  }
}
