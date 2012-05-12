{
	"targets": [
		{
			"target_name": "Engine",
			"type": "none",
			"configurations": {
				"Debug": {},
				"Release": {}
			},

			"all_dependent_settings": {
				"include_dirs": [ "Inc" ]
			},
			"direct_dependent_settings": {
				"conditions": [
					["OS == 'linux'", {
						"libraries": [ "-lEngine" ]
					}],
					["OS == 'win'", {
						"libraries": [ "-lEngine.lib" ],
						"msvs_settings": {
							"VCLinkerTool": {
								"AdditionalLibraryDirectories": [
									">(DEPTH)/Engine/Lib"
								]
							}
						}
					}]
				]
			}
		}
	]
}
