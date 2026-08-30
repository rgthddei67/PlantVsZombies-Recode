#include "OpenGLApi.h"

#include <SDL2/SDL.h>

namespace pvz {

	bool OpenGLApi::Load(std::string& error) {
#define PVZ_GL_LOAD(member, symbol)                                                     \
		do {                                                                                 \
			member = reinterpret_cast<decltype(member)>(SDL_GL_GetProcAddress(#symbol));          \
			if (!member) {                                                                       \
				error = std::string("缺少 OpenGL 3.3 Core 入口: ") + #symbol;                        \
				return false;                                                                        \
			}                                                                                   \
		} while (0)

		PVZ_GL_LOAD(GetString, glGetString);
		PVZ_GL_LOAD(GetIntegerv, glGetIntegerv);
		PVZ_GL_LOAD(GetError, glGetError);
		PVZ_GL_LOAD(Enable, glEnable);
		PVZ_GL_LOAD(Disable, glDisable);
		PVZ_GL_LOAD(BlendEquationSeparate, glBlendEquationSeparate);
		PVZ_GL_LOAD(BlendFuncSeparate, glBlendFuncSeparate);
		PVZ_GL_LOAD(Viewport, glViewport);
		PVZ_GL_LOAD(ClearColor, glClearColor);
		PVZ_GL_LOAD(Clear, glClear);
		PVZ_GL_LOAD(ReadBuffer, glReadBuffer);
		PVZ_GL_LOAD(ReadPixels, glReadPixels);
		PVZ_GL_LOAD(PixelStorei, glPixelStorei);
		PVZ_GL_LOAD(GenTextures, glGenTextures);
		PVZ_GL_LOAD(DeleteTextures, glDeleteTextures);
		PVZ_GL_LOAD(BindTexture, glBindTexture);
		PVZ_GL_LOAD(TexImage2D, glTexImage2D);
		PVZ_GL_LOAD(TexSubImage2D, glTexSubImage2D);
		PVZ_GL_LOAD(TexParameteri, glTexParameteri);
		PVZ_GL_LOAD(GenerateMipmap, glGenerateMipmap);
		PVZ_GL_LOAD(ActiveTexture, glActiveTexture);
		PVZ_GL_LOAD(GenVertexArrays, glGenVertexArrays);
		PVZ_GL_LOAD(DeleteVertexArrays, glDeleteVertexArrays);
		PVZ_GL_LOAD(BindVertexArray, glBindVertexArray);
		PVZ_GL_LOAD(GenBuffers, glGenBuffers);
		PVZ_GL_LOAD(DeleteBuffers, glDeleteBuffers);
		PVZ_GL_LOAD(BindBuffer, glBindBuffer);
		PVZ_GL_LOAD(BufferData, glBufferData);
		PVZ_GL_LOAD(BufferSubData, glBufferSubData);
		PVZ_GL_LOAD(EnableVertexAttribArray, glEnableVertexAttribArray);
		PVZ_GL_LOAD(VertexAttribPointer, glVertexAttribPointer);
		PVZ_GL_LOAD(VertexAttribIPointer, glVertexAttribIPointer);
		PVZ_GL_LOAD(CreateShader, glCreateShader);
		PVZ_GL_LOAD(ShaderSource, glShaderSource);
		PVZ_GL_LOAD(CompileShader, glCompileShader);
		PVZ_GL_LOAD(GetShaderiv, glGetShaderiv);
		PVZ_GL_LOAD(GetShaderInfoLog, glGetShaderInfoLog);
		PVZ_GL_LOAD(DeleteShader, glDeleteShader);
		PVZ_GL_LOAD(CreateProgram, glCreateProgram);
		PVZ_GL_LOAD(AttachShader, glAttachShader);
		PVZ_GL_LOAD(LinkProgram, glLinkProgram);
		PVZ_GL_LOAD(GetProgramiv, glGetProgramiv);
		PVZ_GL_LOAD(GetProgramInfoLog, glGetProgramInfoLog);
		PVZ_GL_LOAD(DeleteProgram, glDeleteProgram);
		PVZ_GL_LOAD(UseProgram, glUseProgram);
		PVZ_GL_LOAD(GetUniformLocation, glGetUniformLocation);
		PVZ_GL_LOAD(Uniform1i, glUniform1i);
		PVZ_GL_LOAD(Uniform1f, glUniform1f);
		PVZ_GL_LOAD(UniformMatrix4fv, glUniformMatrix4fv);
		PVZ_GL_LOAD(DrawElements, glDrawElements);
		PVZ_GL_LOAD(DrawArrays, glDrawArrays);
		PVZ_GL_LOAD(BindBufferBase, glBindBufferBase);
		PVZ_GL_LOAD(GenFramebuffers, glGenFramebuffers);
		PVZ_GL_LOAD(DeleteFramebuffers, glDeleteFramebuffers);
		PVZ_GL_LOAD(BindFramebuffer, glBindFramebuffer);
		PVZ_GL_LOAD(FramebufferTexture2D, glFramebufferTexture2D);
		PVZ_GL_LOAD(CheckFramebufferStatus, glCheckFramebufferStatus);
		PVZ_GL_LOAD(BlitFramebuffer, glBlitFramebuffer);

#undef PVZ_GL_LOAD
		return true;
	}

} // namespace pvz
