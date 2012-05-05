{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "SDLDrv",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*",
				"../Deps/SDL-gyp/SDL.gyp:SDL2"
			],
			"sources": [
				"Src/SDLDrv.cpp",
				"Src/SDLClient.cpp",
				"Src/SDLViewport.cpp"
			]
		}
	]
}
