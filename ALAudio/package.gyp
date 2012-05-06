{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "ALAudio",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*",
				"../Deps/openal-soft-gyp/openal-soft.gyp:*",
				"../Deps/alure-gyp/alure.gyp:*"
			],
			"libraries": [
				"-lmikmod"
			],
			"sources": [
				"Src/ALAudio.cpp",
				"Src/ALAudioSubsystem.cpp",
				"Src/ALAudioMusic.cpp"
			]
		}
	]
}
