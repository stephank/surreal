{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "SDLLaunch",
			"type": "executable",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*",
				"../Deps/SDL-gyp/SDL.gyp:*"
			],
			"sources": [
				"Src/SDLLaunch.cpp"
			]
		}
	]
}
