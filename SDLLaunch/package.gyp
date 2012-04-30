{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "SDLLaunch",
			"type": "executable",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*"
			],
			"libraries": [
				"-lSDL",
			],
			"sources": [
				"Src/SDLLaunch.cpp"
			],
			"conditions": [
				["OS == 'win'", {
					"libraries": [ "-lSDLmain" ]
				}]
			]
		}
	]
}
