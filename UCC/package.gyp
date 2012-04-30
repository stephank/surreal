{
	"includes": ["../common.gypi"],
	"targets": [
		{
			"target_name": "UCC",
			"type": "executable",
			"dependencies": [
				"../Core/package.gyp:*",
				"../Engine/package.gyp:*"
			],
			"sources": [
				"Src/UCC.cpp"
			]
		}
	]
}
