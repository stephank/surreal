{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "OpenGLDrv",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*",
				"../Deps/SDL-gyp/SDL.gyp:SDL2"
			],
			"defines": [
				"UTGLR_UT_BUILD=1"
			],
			"sources": [
				"Src/OpenGLDrv.cpp",
				"Src/OpenGL.cpp",
				"Src/c_gclip.cpp"
			]
		}
	]
}
