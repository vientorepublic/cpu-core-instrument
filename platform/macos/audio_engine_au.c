#include <AudioUnit/AudioUnit.h>
#include <stdlib.h>
#include <string.h>

#include "audio_engine.h"
#include "synth.h"

struct AudioEngine {
  AudioUnit output_unit;
  SharedCoreState* shared_state;
  SynthState synth;
  CoreUsageFrame local_frame;
};

static OSStatus render_callback(void* in_ref_con,
                                AudioUnitRenderActionFlags* io_action_flags,
                                const AudioTimeStamp* in_time_stamp,
                                UInt32 in_bus_number, UInt32 in_number_frames,
                                AudioBufferList* io_data) {
  (void)io_action_flags;
  (void)in_time_stamp;
  (void)in_bus_number;

  AudioEngine* engine = (AudioEngine*)in_ref_con;

  uint64_t seq =
      atomic_load_explicit(&engine->shared_state->seq, memory_order_acquire);
  int idx = (int)(seq & 1ULL);
  engine->local_frame = engine->shared_state->frames[idx];

  if (!io_data || io_data->mNumberBuffers < 1) {
    return noErr;
  }

  float* left = (float*)io_data->mBuffers[0].mData;
  float* right = left;
  if (io_data->mNumberBuffers > 1 && io_data->mBuffers[1].mData) {
    right = (float*)io_data->mBuffers[1].mData;
  }

  synth_render(&engine->synth, &engine->local_frame, left, right,
               (size_t)in_number_frames);
  return noErr;
}

AudioEngine* audio_engine_create(SharedCoreState* shared_state) {
  if (!shared_state) {
    return NULL;
  }

  AudioEngine* engine = (AudioEngine*)calloc(1, sizeof(AudioEngine));
  if (!engine) {
    return NULL;
  }

  engine->shared_state = shared_state;

  AudioComponentDescription desc;
  memset(&desc, 0, sizeof(desc));
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;

  AudioComponent component = AudioComponentFindNext(NULL, &desc);
  if (!component) {
    free(engine);
    return NULL;
  }

  if (AudioComponentInstanceNew(component, &engine->output_unit) != noErr) {
    free(engine);
    return NULL;
  }

  AudioStreamBasicDescription asbd;
  memset(&asbd, 0, sizeof(asbd));
  asbd.mSampleRate = 48000.0;
  asbd.mFormatID = kAudioFormatLinearPCM;
  asbd.mFormatFlags =
      kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
  asbd.mBitsPerChannel = 32;
  asbd.mChannelsPerFrame = 2;
  asbd.mFramesPerPacket = 1;
  asbd.mBytesPerFrame = sizeof(float);
  asbd.mBytesPerPacket = sizeof(float);

  if (AudioUnitSetProperty(engine->output_unit, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Input, 0, &asbd,
                           sizeof(asbd)) != noErr) {
    AudioComponentInstanceDispose(engine->output_unit);
    free(engine);
    return NULL;
  }

  AURenderCallbackStruct callback;
  callback.inputProc = render_callback;
  callback.inputProcRefCon = engine;

  if (AudioUnitSetProperty(
          engine->output_unit, kAudioUnitProperty_SetRenderCallback,
          kAudioUnitScope_Global, 0, &callback, sizeof(callback)) != noErr) {
    AudioComponentInstanceDispose(engine->output_unit);
    free(engine);
    return NULL;
  }

  UInt32 max_frames = 1024;
  AudioUnitSetProperty(
      engine->output_unit, kAudioUnitProperty_MaximumFramesPerSlice,
      kAudioUnitScope_Global, 0, &max_frames, sizeof(max_frames));

  if (AudioUnitInitialize(engine->output_unit) != noErr) {
    AudioComponentInstanceDispose(engine->output_unit);
    free(engine);
    return NULL;
  }

  synth_init(&engine->synth, (float)asbd.mSampleRate);
  return engine;
}

void audio_engine_destroy(AudioEngine* engine) {
  if (!engine) {
    return;
  }
  audio_engine_stop(engine);
  if (engine->output_unit) {
    AudioUnitUninitialize(engine->output_unit);
    AudioComponentInstanceDispose(engine->output_unit);
  }
  free(engine);
}

int audio_engine_start(AudioEngine* engine) {
  if (!engine) {
    return -1;
  }
  return AudioOutputUnitStart(engine->output_unit) == noErr ? 0 : -1;
}

void audio_engine_stop(AudioEngine* engine) {
  if (!engine || !engine->output_unit) {
    return;
  }
  AudioOutputUnitStop(engine->output_unit);
}
