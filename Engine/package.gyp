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
				"libraries": [
					"-lEngine"
				]
			}
		}
	]
}
