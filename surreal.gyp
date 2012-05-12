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
				"SDLLaunch/package.gyp:*",
				"SDLDrv/package.gyp:*",
				"OpenGLDrv/package.gyp:*",
				"ALAudio/package.gyp:*",
				"UCC/package.gyp:*"
			]
		}
	]
}
