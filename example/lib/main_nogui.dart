import 'dart:math' as math;
import 'dart:async';
import 'dart:typed_data';

import 'package:mp_audio_stream/mp_audio_stream.dart';

void main() async {
  final audioStream = getAudioStream();
  audioStream.init();

  const freq = 440;
  const rate = 48000;

  final sineWave = List.generate(
      rate * 1, (i) => math.sin(2 * math.pi * ((i * freq) % rate) / rate));

  audioStream.push(Float32List.fromList(sineWave), 1);

  await Future.delayed(const Duration(seconds: 2));

  audioStream.uninit();
}
