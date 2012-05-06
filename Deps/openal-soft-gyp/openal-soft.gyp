{
	"variables": {
		"library%": "shared_library",
		"openal_dir%": "../openal-soft"
	},

	# FIXME: Needs Windows and OS X love.

	"target_defaults": {
		"include_dirs": [
			"include",
			"<(openal_dir)/include",
			"<(openal_dir)/OpenAL32/Include"
		],

		"cflags": [ "-Winline", "-Wall", "-Wextra" ],
		"defines": [ "AL_BUILD_LIBRARY", "AL_ALEXT_PROTOTYPES", "_GNU_SOURCE=1" ],

		"target_conditions": [
			["_type == 'shared_library'", {
				"cflags": [ "-fPIC" ]
			}],
			["_type == 'static_library'", {
				"defines": [ "AL_LIBTYPE_STATIC" ],
				"all_dependent_settings": {
					"defines": [ "AL_LIBTYPE_STATIC" ]
				}
			}]
		],

		"default_configuration": "Release",
		"configurations": {
			"Debug": {
				"cflags": [ "-g3" ],
				"defines": [ "_DEBUG" ]
			},
			"Release": {
				"cflags": [ "-O2", "-fomit-frame-pointer" ],
				"defines": [ "NDEBUG" ]
			}
		}
	},

	"targets": [
		{
			"conditions": [
				["OS == 'win'", {
					"target_name": "OpenAL32"
				}],
				["OS != 'win'", {
					"target_name": "openal"
				}]
			],
			"type": "<(library)",
			"product_dir": "../../System",
			"cflags": [ "-fvisibility=internal", "-pthread" ],
			"libraries": [ "-lrt", "-lpthread", "-ldl", "-lm" ],
			"sources": [
				"<(openal_dir)/OpenAL32/alAuxEffectSlot.c",
				"<(openal_dir)/OpenAL32/alBuffer.c",
				"<(openal_dir)/OpenAL32/alEffect.c",
				"<(openal_dir)/OpenAL32/alError.c",
				"<(openal_dir)/OpenAL32/alExtension.c",
				"<(openal_dir)/OpenAL32/alFilter.c",
				"<(openal_dir)/OpenAL32/alListener.c",
				"<(openal_dir)/OpenAL32/alSource.c",
				"<(openal_dir)/OpenAL32/alState.c",
				"<(openal_dir)/OpenAL32/alThunk.c",
				"<(openal_dir)/Alc/ALc.c",
				"<(openal_dir)/Alc/ALu.c",
				"<(openal_dir)/Alc/alcConfig.c",
				"<(openal_dir)/Alc/alcDedicated.c",
				"<(openal_dir)/Alc/alcEcho.c",
				"<(openal_dir)/Alc/alcModulator.c",
				"<(openal_dir)/Alc/alcReverb.c",
				"<(openal_dir)/Alc/alcRing.c",
				"<(openal_dir)/Alc/alcThread.c",
				"<(openal_dir)/Alc/bs2b.c",
				"<(openal_dir)/Alc/helpers.c",
				"<(openal_dir)/Alc/hrtf.c",
				"<(openal_dir)/Alc/mixer.c",
				"<(openal_dir)/Alc/panning.c",
				"<(openal_dir)/Alc/backends/loopback.c",
				"<(openal_dir)/Alc/backends/null.c",
				"<(openal_dir)/Alc/backends/alsa.c",
				"<(openal_dir)/Alc/backends/pulseaudio.c"
			],
			"all_dependent_settings": {
				"include_dirs": [ "<(openal_dir)/include" ]
			}
		}
	]
}
