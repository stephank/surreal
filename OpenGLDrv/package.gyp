{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "OpenGLDrv",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*"
			],
			"defines": [
				"UTGLR_UT_BUILD=1"
			],
			"libraries": [
				"-lSDL"
			],
			"sources": [
				"Src/OpenGLDrv.cpp",
				"Src/OpenGL.cpp",
				"Src/c_gclip.cpp"
			]
		}
	]
}
