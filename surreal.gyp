# Dummy target that simply depends on packages.
{
	"targets": [
		{
			"target_name": "RootMeta",
			"type": "none",
			"default_configuration": "Release",
			"configurations": {
				"Debug": {},
				"Release": {}
			},

			"dependencies": [
				"MiniLaunch/package.gyp:*",
				"SDLLaunch/package.gyp:*",
				"SDLDrv/package.gyp:*",
				"OpenGLDrv/package.gyp:*",
				"ALAudio/package.gyp:*",
				"UCC/package.gyp:*"
			],
			"conditions": [
				["OS == 'win'", {
					"dependencies": [
						"D3DDrv/package.gyp:*"
						"D3D9Drv/package.gyp:*"
					]
				}]
			]
		}
	]
}
