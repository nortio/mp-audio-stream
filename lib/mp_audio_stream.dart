// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

/// A multi-platform audio stream output library for real-time generated wave data
library audio_stream;

import 'dart:isolate';
import 'dart:typed_data';

import 'src/mp_audio_stream_mastream.dart'
    if (dart.library.html) 'src/mp_audio_stream_web.dart';

/// Contol class for AudioStream. Use `getAudioStream()` to get its instance.
abstract class AudioStream {
  /// Initializes an audio stream and starts to play. Returns 0 then scucess.
  /// Calling more than once makes a new AudioStream, the previous device will be `uninit`ed.
  ReceivePort getPort();

  int init(
      {int bufferMilliSec = 3000,
      int waitingBufferMilliSec = 100,
      int channels = 1,
      int sampleRate = 44100});

  /// Release current audio stream.
  void uninit();

  /// Resumes audio stream.
  /// For web platform, you should call this from some user-action to activate `AudioContext`.
  /// Ignored on platforms other than web, but recommended to call this to keep multi-platform ready.
  void resume();

  /// Pushes wave data (float32, -1.0 to 1.0) into audio stream. When buffer is full, the input is ignored.
  int push(Float32List buf, int userId);
  int opusPush(Uint8List buf, int userId);
  void removeBuffer(int userId);

  Float32List getMicData(int length);
/*   /// Returns statistics about buffer full/exhaust between the last reset and now
  AudioStreamStat stat(int userId);

  /// Resets all statistics as zero
  void resetStat(); */

  bool isReady(int length);

  Stream<Float32List> inputStream();
}

/// Returns an `AudioStream` instance for running platform (web/others)
AudioStream getAudioStream() => AudioStreamImpl();

/// Statistics about buffer full/exhaust
class AudioStreamStat {
  final int userId;
  final int full;
  final int exhaust;
  AudioStreamStat(
      {required this.full, required this.exhaust, required this.userId});

  factory AudioStreamStat.empty() =>
      AudioStreamStat(full: 0, exhaust: 0, userId: 0);
}
