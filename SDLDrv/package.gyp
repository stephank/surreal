{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "SDLDrv",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*"
			],
			"libraries": [
				"-lSDL"
			],
			"sources": [
				"Src/SDLDrv.cpp",
				"Src/SDLClient.cpp",
				"Src/SDLViewport.cpp"
			]
		}
	]
}
