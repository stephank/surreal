{
	"variables": {
		"library%": "shared_library",
		"alure_dir%": "../alure"
	},

	# FIXME: Needs Windows and OS X love.

	"target_defaults": {
		"include_dirs": [
			"include",
			"<(alure_dir)/include"
		],

		"cflags": [
			"-Wall", "-Wextra",
			"-funswitch-loops"
		],
		"defines": [
			"ALURE_BUILD_LIBRARY",
			"_GNU_SOURCE=1",
			"HAVE_CONFIG_H"
		],

		"target_conditions": [
			["_type == 'shared_library'", {
				"cflags": [ "-fPIC" ]
			}],
			["_type == 'static_library'", {
				"defines": [ "ALURE_STATIC_LIBRARY" ],
				"all_dependent_settings": {
					"defines": [ "ALURE_STATIC_LIBRARY" ]
				}
			}]
		],

		"default_configuration": "Release",
		"configurations": {
			"Debug": {
				"cflags": [ "-g3" ]
			},
			"Release": {
				"cflags": [ "-g", "-O2" ]
			}
		}
	},

	"targets": [
		{
			"conditions": [
				["OS == 'win'", {
					"target_name": "ALURE32"
				}],
				["OS != 'win'", {
					"target_name": "alure"
				}]
			],
			"type": "<(library)",
			"product_dir": "../../System",
			"cflags": [ "-fvisibility=hidden", "-pthread" ],
			"libraries": [ "-lm", "-lpthread" ],
			"dependencies": [
				"../openal-soft-gyp/openal-soft.gyp:*"
			],
			"sources": [
				"<(alure_dir)/src/alure.cpp",
				"<(alure_dir)/src/buffer.cpp",
				"<(alure_dir)/src/istream.cpp",
				"<(alure_dir)/src/stream.cpp",
				"<(alure_dir)/src/streamdec.cpp",
				"<(alure_dir)/src/streamplay.cpp",
				"<(alure_dir)/src/codec_wav.cpp",
				"<(alure_dir)/src/codec_aiff.cpp"
			],
			"all_dependent_settings": {
				"include_dirs": [ "<(alure_dir)/include" ]
			}
		}
	]
}
