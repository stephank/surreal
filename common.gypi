{
	"variables": {
		# Build shared libraries by default.
		"library%": "shared_library",
		# This is a silly hack to delay path transformation until after including,
		# but before dependent settings are merged.
		"package_includes": "Inc"
	},

	"target_defaults": {
		"type": "<(library)",
		"product_prefix": "",
		"product_dir": "System",

		"include_dirs": [ "<(package_includes)" ],
		"all_dependent_settings": {
			"include_dirs": [ "<(package_includes)" ]
		},

		"cflags": [
			"-O2", "-Werror",
			# FIXME: offsetof is incorrectly used to set a C++ UProperty.
			"-Wno-error=invalid-offsetof",
			# FIXME: some APIs (SDL_putenv, mikmod) incorrectly don't take const char*.
			"-Wno-error=write-strings"
		],
		"msvs_cygwin_shell": 0,
		"msvs_settings": {
			"VCCLCompilerTool": {
				"StructMemberAlignment": 3,
				"BufferSecurityCheck": "false",
				"FloatingPointModel": 2,
				"TreatWChar_tAsBuiltInType": "false",
				"WarningLevel": 4,
				"SuppressStartupBanner": "true"
			},
			"VCLinkerTool": {
				"SuppressStartupBanner": "true",
				"GenerateMapFile": "true",
				"RandomizedBaseAddress": 1,
				"DataExecutionPrevention": 0,
				"TargetMachine": 1
			}
		},

		"conditions": [
			["library == 'static_library'", {
				"defines": [ "__STATIC_LINK=1" ]
			}]
		],
		"target_conditions": [
			["_type == 'shared_library'", {
				"cflags": [ "-fPIC" ]
			}],
			["OS == 'win'", {
				"defines": [
					"WIN32", "UNICODE", "_UNICODE",
					"ThisPackage=>(_target_name)"
				]
			}],
			["OS != 'win'", {
				"defines": [ "GPackage=GPackage>(_target_name)" ]
			}],
			["OS == 'linux'", {
				"defines": [ "__LINUX__", "_REENTRANT" ]
			}]
		],

		"default_configuration": "Release",
		"configurations": {
			"Debug": {
				"defines": [ "_REALLY_WANT_DEBUG" ],
				"cflags": [ "-ggdb" ],
				"msvs_settings": {
					"VCCLCompilerTool": {
						"Optimization": 1,
						"InlineFunctionExpansion": 0,
						"MinimalRebuild": "true",
						"BasicRuntimeChecks": 0,
						"DebugInformationFormat": 3,
						"RuntimeLibrary": 3
					},
					"VCLinkerTool": {
						"LinkIncremental": 2,
						"GenerateDebugInformation": "true"
					}
				}
			},
			"Release": {
				"cflags": [ "-fomit-frame-pointer" ],
				"msvs_settings": {
					"VCCLCompilerTool": {
						"Optimization": 2,
						"InlineFunctionExpansion": 1,
						"StringPooling": "true",
						"EnableFunctionLevelLinking": "true",
						"AssemblerOutput": 4,
						"DebugInformationFormat": 0,
						"RuntimeLibrary": 2
					},
					"VCLinkerTool": {
						"LinkIncremental": 1
					}
				}
			}
		}
	}
}
