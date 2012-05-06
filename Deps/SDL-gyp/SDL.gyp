{
	"variables": {
		"library%": "shared_library",
		"sdl_dir%": "../SDL"
	},

	# FIXME: Needs Windows and OS X love.

	"target_defaults": {
		"include_dirs": [ "include", "<(sdl_dir)/include" ],
		"cflags": [ "-g", "-fvisibility=hidden", "-Wall" ],
		"defines": [ "__LINUX__", "_REENTRANT" ],
		"libraries": [ "-lm", "-ldl", "-lpthread" ],

		"target_conditions": [
			["_type == 'shared_library'", {
				"cflags": [ "-fPIC" ]
			}]
		],

		"default_configuration": "Release",
		"configurations": {
			"Debug": {
				"cflags": [ "-O0" ]
			},
			"Release": {
				"cflags": [ "-O3" ]
			}
		}
	},

	"targets": [
		{
			"target_name": "SDL2",
			"type": "<(library)",
			"sources": [
				"<(sdl_dir)/src/SDL_assert.c",
				"<(sdl_dir)/src/SDL.c",
				"<(sdl_dir)/src/SDL_error.c",
				"<(sdl_dir)/src/SDL_fatal.c",
				"<(sdl_dir)/src/SDL_hints.c",
				"<(sdl_dir)/src/SDL_log.c",
				"<(sdl_dir)/src/atomic/SDL_atomic.c",
				"<(sdl_dir)/src/atomic/SDL_spinlock.c",
				"<(sdl_dir)/src/audio/SDL_audio.c",
				"<(sdl_dir)/src/audio/SDL_audiocvt.c",
				"<(sdl_dir)/src/audio/SDL_audiodev.c",
				"<(sdl_dir)/src/audio/SDL_audiotypecvt.c",
				"<(sdl_dir)/src/audio/SDL_mixer.c",
				"<(sdl_dir)/src/audio/SDL_wave.c",
				"<(sdl_dir)/src/cpuinfo/SDL_cpuinfo.c",
				"<(sdl_dir)/src/events/SDL_clipboardevents.c",
				"<(sdl_dir)/src/events/SDL_dropevents.c",
				"<(sdl_dir)/src/events/SDL_events.c",
				"<(sdl_dir)/src/events/SDL_gesture.c",
				"<(sdl_dir)/src/events/SDL_keyboard.c",
				"<(sdl_dir)/src/events/SDL_mouse.c",
				"<(sdl_dir)/src/events/SDL_quit.c",
				"<(sdl_dir)/src/events/SDL_touch.c",
				"<(sdl_dir)/src/events/SDL_windowevents.c",
				"<(sdl_dir)/src/file/SDL_rwops.c",
				"<(sdl_dir)/src/render/SDL_render.c",
				"<(sdl_dir)/src/render/SDL_yuv_mmx.c",
				"<(sdl_dir)/src/render/SDL_yuv_sw.c",
				"<(sdl_dir)/src/render/direct3d/SDL_render_d3d.c",
				"<(sdl_dir)/src/render/nds/SDL_libgl2D.c",
				"<(sdl_dir)/src/render/nds/SDL_ndsrender.c",
				"<(sdl_dir)/src/render/opengles2/SDL_render_gles2.c",
				"<(sdl_dir)/src/render/opengles2/SDL_shaders_gles2.c",
				"<(sdl_dir)/src/render/opengles/SDL_render_gles.c",
				"<(sdl_dir)/src/render/opengl/SDL_render_gl.c",
				"<(sdl_dir)/src/render/opengl/SDL_shaders_gl.c",
				"<(sdl_dir)/src/render/software/SDL_blendfillrect.c",
				"<(sdl_dir)/src/render/software/SDL_blendline.c",
				"<(sdl_dir)/src/render/software/SDL_blendpoint.c",
				"<(sdl_dir)/src/render/software/SDL_drawline.c",
				"<(sdl_dir)/src/render/software/SDL_drawpoint.c",
				"<(sdl_dir)/src/render/software/SDL_render_sw.c",
				"<(sdl_dir)/src/stdlib/SDL_getenv.c",
				"<(sdl_dir)/src/stdlib/SDL_iconv.c",
				"<(sdl_dir)/src/stdlib/SDL_malloc.c",
				"<(sdl_dir)/src/stdlib/SDL_qsort.c",
				"<(sdl_dir)/src/stdlib/SDL_stdlib.c",
				"<(sdl_dir)/src/stdlib/SDL_string.c",
				"<(sdl_dir)/src/thread/SDL_thread.c",
				"<(sdl_dir)/src/timer/SDL_timer.c",
				"<(sdl_dir)/src/video/SDL_blit_0.c",
				"<(sdl_dir)/src/video/SDL_blit_1.c",
				"<(sdl_dir)/src/video/SDL_blit_A.c",
				"<(sdl_dir)/src/video/SDL_blit_auto.c",
				"<(sdl_dir)/src/video/SDL_blit.c",
				"<(sdl_dir)/src/video/SDL_blit_copy.c",
				"<(sdl_dir)/src/video/SDL_blit_N.c",
				"<(sdl_dir)/src/video/SDL_blit_slow.c",
				"<(sdl_dir)/src/video/SDL_bmp.c",
				"<(sdl_dir)/src/video/SDL_clipboard.c",
				"<(sdl_dir)/src/video/SDL_fillrect.c",
				"<(sdl_dir)/src/video/SDL_pixels.c",
				"<(sdl_dir)/src/video/SDL_rect.c",
				"<(sdl_dir)/src/video/SDL_RLEaccel.c",
				"<(sdl_dir)/src/video/SDL_shape.c",
				"<(sdl_dir)/src/video/SDL_stretch.c",
				"<(sdl_dir)/src/video/SDL_surface.c",
				"<(sdl_dir)/src/video/SDL_video.c",
				"<(sdl_dir)/src/video/x11/imKStoUCS.c",
				"<(sdl_dir)/src/video/x11/SDL_x11clipboard.c",
				"<(sdl_dir)/src/video/x11/SDL_x11dyn.c",
				"<(sdl_dir)/src/video/x11/SDL_x11events.c",
				"<(sdl_dir)/src/video/x11/SDL_x11framebuffer.c",
				"<(sdl_dir)/src/video/x11/SDL_x11keyboard.c",
				"<(sdl_dir)/src/video/x11/SDL_x11modes.c",
				"<(sdl_dir)/src/video/x11/SDL_x11mouse.c",
				"<(sdl_dir)/src/video/x11/SDL_x11opengl.c",
				"<(sdl_dir)/src/video/x11/SDL_x11opengles.c",
				"<(sdl_dir)/src/video/x11/SDL_x11shape.c",
				"<(sdl_dir)/src/video/x11/SDL_x11touch.c",
				"<(sdl_dir)/src/video/x11/SDL_x11video.c",
				"<(sdl_dir)/src/video/x11/SDL_x11window.c",
				"<(sdl_dir)/src/loadso/dlopen/SDL_sysloadso.c"
			],
			"all_dependent_settings": {
				"include_dirs": [ "include", "<(sdl_dir)/include" ]
			},
			"direct_dependent_settings": {
				"conditions": [
					["_type == 'executable'", {
						"dependencies": [ "SDL2main" ]
					}]
				]
			}
		},

		{
			"target_name": "SDL2main",
			"type": "static_library",
			"sources": [
				"<(sdl_dir)/src/main/dummy/SDL_dummy_main.c"
			]
		}
	]
}
