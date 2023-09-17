import 'dart:async';
import 'dart:js' as js;
import 'dart:html' as html;
import 'dart:typed_data';

import 'package:flutter/services.dart';

import '../mp_audio_stream.dart';

Future<String> _jsText() async =>
    await rootBundle.loadString('packages/mp_audio_stream/js/audio_stream.js');

/// Contol class for AudioStream on web platform. Use `getAudioStream()` to get its instance.
class AudioStreamImpl extends AudioStream {
  final _streamCompleter = Completer<js.JsObject>();
  js.JsObject? _stream;

  AudioStreamImpl() {
    (() async {
      final scriptTag = html.ScriptElement()..text = await _jsText();
      html.document.head?.children.add(scriptTag);
      _stream = js.context['AudioStream'];
      _streamCompleter.complete(_stream);
    })();
  }

  void _callLater(String method, List args) {
    _streamCompleter.future.then((s) => s.callMethod(method, args));
  }

  @override
  int init(
      {int bufferMilliSec = 3000,
      int waitingBufferMilliSec = 100,
      int channels = 1,
      int sampleRate = 44100}) {
    _callLater('init', [
      bufferMilliSec * sampleRate / 1000,
      waitingBufferMilliSec * sampleRate / 1000,
      channels,
      sampleRate
    ]);
    return 0;
  }

  @override
  void uninit() {
    _callLater('uninit', []);
  }

  @override
  void resume() {
    _callLater('resume', []);
  }

  @override
  int push(Float32List buf, int userId) {
    _stream?.callMethod('push', [buf]);
    return 0;
  }

  @override
  AudioStreamStat stat(int userId) {
    if (_stream != null) {
      final statJsObj = _stream!['stat'];
      final fullCount = statJsObj['fullCount'];
      final exhaustCount = statJsObj['exhaustCount'];
      return AudioStreamStat(
          full: fullCount, exhaust: exhaustCount, userId: userId);
    } else {
      return AudioStreamStat.empty();
    }
  }

  @override
  void resetStat() {
    _stream?.callMethod('resetStat', []);
  }

  @override
  void removeBuffer(int userId) {
    // TODO: implement removeBuffer
  }

  @override
  Float32List getMicData(int length) {
    // TODO: implement getMicData
    throw UnimplementedError();
  }

  @override
  bool isReady(int length) {
    // TODO: implement isReady
    throw UnimplementedError();
  }

  @override
  Stream<Float32List> inputStream() {
    // TODO: implement inputStream
    throw UnimplementedError();
  }
}
