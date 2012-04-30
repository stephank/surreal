{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "D3DDrv",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*"
			],
			"libraries": [
				"-lDxGuid",
				"-ld3dim"
			],
			"sources": [
				"Src/D3DDrv.cpp",
				"Src/Direct3D7.cpp",
				"Src/und3d.cpp"
			]
		}
	]
}
