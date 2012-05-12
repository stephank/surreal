{
	"targets": [
		{
			"target_name": "Core",
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
						"libraries": [ "-lCore" ]
					}],
					["OS == 'win'", {
						"libraries": [ "-lCore.lib" ],
						"msvs_settings": {
							"VCLinkerTool": {
								"AdditionalLibraryDirectories": [
									">(DEPTH)/Core/Lib"
								]
							}
						}
					}]
				]
			}
		}
	]
}
