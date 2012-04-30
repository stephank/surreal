{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "MiniLaunch",
			"type": "executable",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*"
			],
			"sources": [
				"Src/MiniLaunch.cpp"
			],
			"conditions": [
				["OS == 'win'", {
					"libraries": [ "-lwinmm" ]
				}]
			]
		}
	]
}
