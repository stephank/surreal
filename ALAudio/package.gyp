{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "ALAudio",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*"
			],
			"libraries": [
				"-lopenal",
				"-lalure",
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
