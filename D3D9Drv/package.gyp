{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "D3D9Drv",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*"
			],
			"libraries": [
				"-lwinmm",
				"-ld3dx9",
				"-ladvapi32"
			],
			"sources": [
				"Src/D3D9Drv.cpp",
				"Src/D3D9.cpp"
			]
		}
	]
}
