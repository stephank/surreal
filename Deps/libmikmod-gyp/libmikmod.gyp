{
	"variables": {
		"library%": "shared_library",
		"mikmod_dir%": "../libmikmod"
	},

	# FIXME: Needs Windows and OS X love.

	"target_defaults": {
		"include_dirs": [
			"include",
			"<(mikmod_dir)/include"
		],

		"cflags": [
			"-Wall",
			"-finline-functions",
			"-funroll-loops",
			"-ffast-math"
		],
		"defines": [
			"HAVE_CONFIG_H",
			"unix"
		],

		"target_conditions": [
			["_type == 'shared_library'", {
				"cflags": [ "-fPIC" ]
			}]
		],

		"default_configuration": "Release",
		"configurations": {
			"Debug": {
				"cflags": [ "-g3", "-Werror" ],
				"defines": [ "MIKMOD_DEBUG" ]
			},
			"Release": {
				"cflags": [ "-g", "-O2" ]
			}
		}
	},

	"targets": [
		{
			"target_name": "mikmod",
			"type": "<(library)",
			"product_dir": "../../System",
			"libraries": [ "-lm" ],
			"sources": [
				"<(mikmod_dir)/loaders/load_669.c",
				"<(mikmod_dir)/loaders/load_amf.c",
				"<(mikmod_dir)/loaders/load_asy.c",
				"<(mikmod_dir)/loaders/load_dsm.c",
				"<(mikmod_dir)/loaders/load_far.c",
				"<(mikmod_dir)/loaders/load_gdm.c",
				"<(mikmod_dir)/loaders/load_gt2.c",
				"<(mikmod_dir)/loaders/load_it.c",
				"<(mikmod_dir)/loaders/load_imf.c",
				"<(mikmod_dir)/loaders/load_m15.c",
				"<(mikmod_dir)/loaders/load_med.c",
				"<(mikmod_dir)/loaders/load_mod.c",
				"<(mikmod_dir)/loaders/load_mtm.c",
				"<(mikmod_dir)/loaders/load_okt.c",
				"<(mikmod_dir)/loaders/load_s3m.c",
				"<(mikmod_dir)/loaders/load_stm.c",
				"<(mikmod_dir)/loaders/load_stx.c",
				"<(mikmod_dir)/loaders/load_ult.c",
				"<(mikmod_dir)/loaders/load_uni.c",
				"<(mikmod_dir)/loaders/load_xm.c",
				"<(mikmod_dir)/mmio/mmalloc.c",
				"<(mikmod_dir)/mmio/mmerror.c",
				"<(mikmod_dir)/mmio/mmio.c",
				"<(mikmod_dir)/playercode/mdriver.c",
				"<(mikmod_dir)/playercode/mdreg.c",
				"<(mikmod_dir)/playercode/mdulaw.c",
				"<(mikmod_dir)/playercode/mloader.c",
				"<(mikmod_dir)/playercode/mlreg.c",
				"<(mikmod_dir)/playercode/mlutil.c",
				"<(mikmod_dir)/playercode/mplayer.c",
				"<(mikmod_dir)/playercode/munitrk.c",
				"<(mikmod_dir)/playercode/mwav.c",
				"<(mikmod_dir)/playercode/npertab.c",
				"<(mikmod_dir)/playercode/sloader.c",
				"<(mikmod_dir)/playercode/virtch.c",
				"<(mikmod_dir)/playercode/virtch2.c",
				"<(mikmod_dir)/playercode/virtch_common.c"
			],
			"all_dependent_settings": {
				"include_dirs": [ "<(mikmod_dir)/include" ]
			}
		}
	]
}
