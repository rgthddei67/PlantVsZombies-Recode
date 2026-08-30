#pragma once

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include <string>

namespace pvz {

	/**
	 * 加载 OpenGL 3.3 Core 基线入口；SSBO 可选快路只复用 3.3 已有的 buffer 入口。
	 */
	struct OpenGLApi {
		using GetStringProc = const GLubyte* (APIENTRY*)(GLenum);
		using GetIntegervProc = void (APIENTRY*)(GLenum, GLint*);
		using GetErrorProc = GLenum (APIENTRY*)();
		using EnableProc = void (APIENTRY*)(GLenum);
		using DisableProc = void (APIENTRY*)(GLenum);
		using BlendEquationSeparateProc = void (APIENTRY*)(GLenum, GLenum);
		using BlendFuncSeparateProc = void (APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
		using ViewportProc = void (APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
		using ClearColorProc = void (APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat);
		using ClearProc = void (APIENTRY*)(GLbitfield);
		using ReadBufferProc = void (APIENTRY*)(GLenum);
		using ReadPixelsProc = void (APIENTRY*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
		using PixelStoreiProc = void (APIENTRY*)(GLenum, GLint);
		using GenTexturesProc = void (APIENTRY*)(GLsizei, GLuint*);
		using DeleteTexturesProc = void (APIENTRY*)(GLsizei, const GLuint*);
		using BindTextureProc = void (APIENTRY*)(GLenum, GLuint);
		using TexImage2DProc = void (APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei,
			GLint, GLenum, GLenum, const void*);
		using TexSubImage2DProc = void (APIENTRY*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei,
			GLenum, GLenum, const void*);
		using TexParameteriProc = void (APIENTRY*)(GLenum, GLenum, GLint);
		using ActiveTextureProc = void (APIENTRY*)(GLenum);
		using GenVertexArraysProc = void (APIENTRY*)(GLsizei, GLuint*);
		using DeleteVertexArraysProc = void (APIENTRY*)(GLsizei, const GLuint*);
		using BindVertexArrayProc = void (APIENTRY*)(GLuint);
		using GenBuffersProc = void (APIENTRY*)(GLsizei, GLuint*);
		using DeleteBuffersProc = void (APIENTRY*)(GLsizei, const GLuint*);
		using BindBufferProc = void (APIENTRY*)(GLenum, GLuint);
		using BufferDataProc = void (APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
		using BufferSubDataProc = void (APIENTRY*)(GLenum, GLintptr, GLsizeiptr, const void*);
		using EnableVertexAttribArrayProc = void (APIENTRY*)(GLuint);
		using VertexAttribPointerProc = void (APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
		using VertexAttribIPointerProc = void (APIENTRY*)(GLuint, GLint, GLenum, GLsizei, const void*);
		using CreateShaderProc = GLuint (APIENTRY*)(GLenum);
		using ShaderSourceProc = void (APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
		using CompileShaderProc = void (APIENTRY*)(GLuint);
		using GetShaderivProc = void (APIENTRY*)(GLuint, GLenum, GLint*);
		using GetShaderInfoLogProc = void (APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
		using DeleteShaderProc = void (APIENTRY*)(GLuint);
		using CreateProgramProc = GLuint (APIENTRY*)();
		using AttachShaderProc = void (APIENTRY*)(GLuint, GLuint);
		using LinkProgramProc = void (APIENTRY*)(GLuint);
		using GetProgramivProc = void (APIENTRY*)(GLuint, GLenum, GLint*);
		using GetProgramInfoLogProc = void (APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
		using DeleteProgramProc = void (APIENTRY*)(GLuint);
		using UseProgramProc = void (APIENTRY*)(GLuint);
		using GetUniformLocationProc = GLint (APIENTRY*)(GLuint, const GLchar*);
		using Uniform1iProc = void (APIENTRY*)(GLint, GLint);
		using Uniform1fProc = void (APIENTRY*)(GLint, GLfloat);
		using UniformMatrix4fvProc = void (APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*);
		using DrawElementsProc = void (APIENTRY*)(GLenum, GLsizei, GLenum, const void*);
		using DrawArraysProc = void (APIENTRY*)(GLenum, GLint, GLsizei);
		using BindBufferBaseProc = void (APIENTRY*)(GLenum, GLuint, GLuint);
		using GenFramebuffersProc = void (APIENTRY*)(GLsizei, GLuint*);
		using DeleteFramebuffersProc = void (APIENTRY*)(GLsizei, const GLuint*);
		using BindFramebufferProc = void (APIENTRY*)(GLenum, GLuint);
		using FramebufferTexture2DProc = void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
		using CheckFramebufferStatusProc = GLenum (APIENTRY*)(GLenum);

		GetStringProc GetString = nullptr;
		GetIntegervProc GetIntegerv = nullptr;
		GetErrorProc GetError = nullptr;
		EnableProc Enable = nullptr;
		DisableProc Disable = nullptr;
		BlendEquationSeparateProc BlendEquationSeparate = nullptr;
		BlendFuncSeparateProc BlendFuncSeparate = nullptr;
		ViewportProc Viewport = nullptr;
		ClearColorProc ClearColor = nullptr;
		ClearProc Clear = nullptr;
		ReadBufferProc ReadBuffer = nullptr;
		ReadPixelsProc ReadPixels = nullptr;
		PixelStoreiProc PixelStorei = nullptr;
		GenTexturesProc GenTextures = nullptr;
		DeleteTexturesProc DeleteTextures = nullptr;
		BindTextureProc BindTexture = nullptr;
		TexImage2DProc TexImage2D = nullptr;
		TexSubImage2DProc TexSubImage2D = nullptr;
		TexParameteriProc TexParameteri = nullptr;
		PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr;
		ActiveTextureProc ActiveTexture = nullptr;
		GenVertexArraysProc GenVertexArrays = nullptr;
		DeleteVertexArraysProc DeleteVertexArrays = nullptr;
		BindVertexArrayProc BindVertexArray = nullptr;
		GenBuffersProc GenBuffers = nullptr;
		DeleteBuffersProc DeleteBuffers = nullptr;
		BindBufferProc BindBuffer = nullptr;
		BufferDataProc BufferData = nullptr;
		BufferSubDataProc BufferSubData = nullptr;
		EnableVertexAttribArrayProc EnableVertexAttribArray = nullptr;
		VertexAttribPointerProc VertexAttribPointer = nullptr;
		VertexAttribIPointerProc VertexAttribIPointer = nullptr;
		CreateShaderProc CreateShader = nullptr;
		ShaderSourceProc ShaderSource = nullptr;
		CompileShaderProc CompileShader = nullptr;
		GetShaderivProc GetShaderiv = nullptr;
		GetShaderInfoLogProc GetShaderInfoLog = nullptr;
		DeleteShaderProc DeleteShader = nullptr;
		CreateProgramProc CreateProgram = nullptr;
		AttachShaderProc AttachShader = nullptr;
		LinkProgramProc LinkProgram = nullptr;
		GetProgramivProc GetProgramiv = nullptr;
		GetProgramInfoLogProc GetProgramInfoLog = nullptr;
		DeleteProgramProc DeleteProgram = nullptr;
		UseProgramProc UseProgram = nullptr;
		GetUniformLocationProc GetUniformLocation = nullptr;
		Uniform1iProc Uniform1i = nullptr;
		Uniform1fProc Uniform1f = nullptr;
		UniformMatrix4fvProc UniformMatrix4fv = nullptr;
		DrawElementsProc DrawElements = nullptr;
		DrawArraysProc DrawArrays = nullptr;
		BindBufferBaseProc BindBufferBase = nullptr;
		GenFramebuffersProc GenFramebuffers = nullptr;
		DeleteFramebuffersProc DeleteFramebuffers = nullptr;
		BindFramebufferProc BindFramebuffer = nullptr;
		FramebufferTexture2DProc FramebufferTexture2D = nullptr;
		CheckFramebufferStatusProc CheckFramebufferStatus = nullptr;
		PFNGLBLITFRAMEBUFFERPROC BlitFramebuffer = nullptr;

		bool Load(std::string& error);
	};

} // namespace pvz
